#include "arquivo.h"
#include "estoque.h"
#include "produto.h"
#include "str16.h"
#include "tui16.h"

#ifndef _WIN32
#error "A interface tui16 deste projeto requer Windows."
#endif

#include <conio.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>

#define APP_ARQUIVO_PRODUTOS "produtos.csv16"
#define APP_ARQUIVO_ESTOQUE "estoque.csv16"
#define APP_MIN_WIDTH 92
#define APP_MIN_HEIGHT 28
#define APP_BOX_WIDTH 108
#define APP_BOX_HEIGHT 31
#define APP_DELAY_MS 25
#define APP_PAGE_SIZE 7U
#define APP_TEXT_CAP 256U
#define APP_NUM_CAP 64U

/*
 * main.c concentra somente a orquestração da aplicação:
 * - navegação entre menus;
 * - coleta e validação de entrada;
 * - confirmação explícita das ações;
 * - apresentação das mensagens de estado;
 * - integração entre estoque, persistência e TUI.
 *
 * As regras de negócio permanecem nos módulos produto.c e estoque.c.
 */

typedef enum AppKey
{
    APP_KEY_NONE = 0,
    APP_KEY_UP,
    APP_KEY_DOWN,
    APP_KEY_LEFT,
    APP_KEY_RIGHT,
    APP_KEY_ENTER,
    APP_KEY_ESC,
    APP_KEY_1,
    APP_KEY_2,
    APP_KEY_3,
    APP_KEY_4,
    APP_KEY_5,
    APP_KEY_6,
    APP_KEY_7,
    APP_KEY_8,
    APP_KEY_9
} AppKey;

typedef struct AppLayout
{
    int row;
    int col;
    int width;
    int height;
} AppLayout;

typedef struct App
{
    Estoque estoque;
    uint32_t limite_baixo;
} App;

static size_t app_length16(const char16_t *texto)
{
    return texto == NULL ? 0U : str16_length(texto);
}

static void app_copy16(char16_t *destino, size_t capacidade, const char16_t *origem)
{
    if (destino == NULL || capacidade == 0U)
        return;

    destino[0] = u'\0';
    if (origem != NULL)
        (void)str16_copy_n(origem, destino, capacidade);
}

static int app_append16(char16_t *destino, size_t capacidade, const char16_t *texto)
{
    return destino != NULL && texto != NULL &&
           str16_concat_n(texto, destino, capacidade);
}

static void app_truncate16(char16_t *texto, size_t maximo)
{
    size_t tamanho;

    if (texto == NULL)
        return;

    tamanho = str16_length(texto);
    if (tamanho <= maximo)
        return;

    if (maximo >= 3U)
    {
        texto[maximo - 3U] = u'.';
        texto[maximo - 2U] = u'.';
        texto[maximo - 1U] = u'.';
    }
    texto[maximo] = u'\0';
}

static int app_ascii_to16(const char *ascii, char16_t *saida, size_t capacidade)
{
    size_t i;

    if (ascii == NULL || saida == NULL || capacidade == 0U)
        return 0;

    for (i = 0U; ascii[i] != '\0'; ++i)
    {
        if (i + 1U >= capacidade)
        {
            saida[0] = u'\0';
            return 0;
        }
        saida[i] = (char16_t)(unsigned char)ascii[i];
    }

    saida[i] = u'\0';
    return 1;
}

static int app_u64_text(uint64_t valor, char16_t *saida, size_t capacidade)
{
    char ascii[32];
    int total;

    total = snprintf(ascii, sizeof(ascii), "%llu", (unsigned long long)valor);
    return total >= 0 && (size_t)total < sizeof(ascii) &&
           app_ascii_to16(ascii, saida, capacidade);
}

static int app_double_text(double valor, char16_t *saida, size_t capacidade)
{
    char ascii[64];
    int total;
    size_t i;

    if (!isfinite(valor))
        return 0;

    total = snprintf(ascii, sizeof(ascii), "%.2f", valor);
    if (total < 0 || (size_t)total >= sizeof(ascii))
        return 0;

    for (i = 0U; i < (size_t)total; ++i)
    {
        if (ascii[i] == ',')
            ascii[i] = '.';
    }

    return app_ascii_to16(ascii, saida, capacidade);
}

static int app_time_text(time_t valor, char16_t *saida, size_t capacidade)
{
    struct tm local;
    char ascii[64];
    size_t total;

#if defined(_WIN32)
    if (localtime_s(&local, &valor) != 0)
        return 0;
#else
    if (localtime_r(&valor, &local) == NULL)
        return 0;
#endif

    total = strftime(ascii, sizeof(ascii), "%d/%m/%Y %H:%M:%S", &local);
    return total > 0U && app_ascii_to16(ascii, saida, capacidade);
}

static int app_parse_u32(const char16_t *texto, uint32_t *valor)
{
    uint64_t acumulado = 0U;
    size_t i = 0U;

    if (texto == NULL || valor == NULL || texto[0] == u'\0')
        return 0;

    while (texto[i] != u'\0')
    {
        uint64_t digito;

        if (texto[i] < u'0' || texto[i] > u'9')
            return 0;

        digito = (uint64_t)(texto[i] - u'0');
        if (acumulado > (UINT32_MAX - digito) / UINT64_C(10))
            return 0;

        acumulado = acumulado * UINT64_C(10) + digito;
        ++i;
    }

    *valor = (uint32_t)acumulado;
    return 1;
}

static int app_parse_double_positivo(const char16_t *texto, double *valor)
{
    char16_t *fim;
    double convertido;

    if (texto == NULL || valor == NULL || texto[0] == u'\0')
        return 0;

    convertido = str16_forDouble(texto, &fim);
    if (fim == NULL || fim == texto || *fim != u'\0' ||
        !isfinite(convertido) || convertido <= 0.0)
    {
        return 0;
    }

    *valor = convertido;
    return 1;
}

static AppKey app_read_key(void)
{
    wint_t ch;

    if (!_kbhit())
        return APP_KEY_NONE;

    ch = _getwch();
    if (ch == 0 || ch == 0xE0)
    {
        switch (_getwch())
        {
        case 72:
            return APP_KEY_UP;
        case 80:
            return APP_KEY_DOWN;
        case 75:
            return APP_KEY_LEFT;
        case 77:
            return APP_KEY_RIGHT;
        default:
            return APP_KEY_NONE;
        }
    }

    if (ch == 13)
        return APP_KEY_ENTER;
    if (ch == 27)
        return APP_KEY_ESC;
    if (ch >= L'1' && ch <= L'9')
        return (AppKey)(APP_KEY_1 + (int)(ch - L'1'));

    return APP_KEY_NONE;
}

static AppKey app_wait_key(void)
{
    AppKey key;

    do
    {
        key = app_read_key();
        if (key == APP_KEY_NONE)
            Sleep(APP_DELAY_MS);
    } while (key == APP_KEY_NONE);

    return key;
}

static int app_layout(AppLayout *layout)
{
    int term_width;
    int term_height;

    if (layout == NULL || !tui16_terminal_size(&term_width, &term_height))
        return 0;

    if (term_width < APP_MIN_WIDTH || term_height < APP_MIN_HEIGHT)
        return 0;

    layout->width = term_width - 2 < APP_BOX_WIDTH ? term_width - 2 : APP_BOX_WIDTH;
    layout->height = term_height - 2 < APP_BOX_HEIGHT ? term_height - 2 : APP_BOX_HEIGHT;
    layout->col = (term_width - layout->width) / 2 + 1;
    layout->row = (term_height - layout->height) / 2 + 1;
    return 1;
}

static int app_wait_terminal(AppLayout *layout)
{
    for (;;)
    {
        int width = 0;
        int height = 0;
        AppKey key;

        if (app_layout(layout))
            return 1;

        tui16_clear();
        (void)tui16_terminal_size(&width, &height);
        tui16_drawtext(2, 2, u"Terminal pequeno demais para a interface.");
        tui16_drawtext(4, 2, u"Aumente a janela para pelo menos 92 colunas por 28 linhas.");
        tui16_drawtext(6, 2, u"ESC encerra a aplicação.");

        key = app_read_key();
        if (key == APP_KEY_ESC)
            return 0;

        (void)width;
        (void)height;
        Sleep(100U);
    }
}

static int app_screen(const char16_t *titulo,
                      const char16_t *subtitulo,
                      const char16_t *rodape,
                      AppLayout *layout)
{
    size_t titulo_len;
    int titulo_col;

    if (!app_wait_terminal(layout))
        return 0;

    tui16_clear();
    tui16_setcolor(96, 40);
    tui16_drawbox(layout->row, layout->col, layout->width, layout->height);
    tui16_resetstyle();

    titulo_len = app_length16(titulo);
    titulo_col = layout->col + (layout->width - (int)titulo_len) / 2;
    tui16_setcolor(96, 40);
    tui16_drawtext(layout->row + 1, titulo_col, titulo);
    tui16_resetstyle();

    if (subtitulo != NULL)
        tui16_drawtext(layout->row + 3, layout->col + 3, subtitulo);

    if (rodape != NULL)
    {
        tui16_setcolor(90, 40);
        tui16_drawtext(layout->row + layout->height - 2, layout->col + 3, rodape);
        tui16_resetstyle();
    }

    return 1;
}

static void app_draw_label_value(const AppLayout *layout,
                                 int relative_row,
                                 const char16_t *label,
                                 const char16_t *value)
{
    char16_t linha[APP_TEXT_CAP];

    linha[0] = u'\0';
    (void)app_append16(linha, APP_TEXT_CAP, label);
    (void)app_append16(linha, APP_TEXT_CAP, value != NULL ? value : u"");
    tui16_drawtext(layout->row + relative_row, layout->col + 4, linha);
}

static void app_message(const char16_t *titulo,
                        const char16_t *mensagem,
                        const char16_t *detalhe)
{
    AppLayout layout;

    if (!app_screen(titulo, mensagem,
                    u"ENTER ou ESC: voltar ao menu anterior", &layout))
        return;

    if (detalhe != NULL)
        tui16_drawtext(layout.row + 6, layout.col + 4, detalhe);

    (void)app_wait_key();
}

static int app_menu(const char16_t *titulo,
                    const char16_t *subtitulo,
                    const char16_t *const *itens,
                    int total,
                    int selecionado)
{
    AppLayout layout;

    if (itens == NULL || total <= 0)
        return -1;

    if (selecionado < 0 || selecionado >= total)
        selecionado = 0;

    for (;;)
    {
        AppKey key;

        if (!app_screen(titulo, subtitulo,
                        u"↑ ↓ navegar | 1-9 atalho | ENTER selecionar | ESC voltar", &layout))
            return -1;

        (void)tui16_listbox(layout.row + 6, layout.col + 6,
                            itens, total, selecionado);

        for (;;)
        {
            key = app_read_key();
            if (key != APP_KEY_NONE)
                break;
            Sleep(APP_DELAY_MS);
        }

        if (key == APP_KEY_UP)
            selecionado = selecionado == 0 ? total - 1 : selecionado - 1;
        else if (key == APP_KEY_DOWN)
            selecionado = (selecionado + 1) % total;
        else if (key == APP_KEY_ENTER)
            return selecionado;
        else if (key >= APP_KEY_1 && key <= APP_KEY_9 &&
                 (int)(key - APP_KEY_1) < total)
            return (int)(key - APP_KEY_1);
        else if (key == APP_KEY_ESC)
            return -1;
    }
}

static int app_confirmar(const char16_t *titulo, const char16_t *mensagem)
{
    static const char16_t *const itens[] = {
        u"1. Confirmar ação",
        u"2. Cancelar ação"};

    return app_menu(titulo, mensagem, itens, 2, 1) == 0;
}

static int app_input_text(const char16_t *titulo,
                          const char16_t *instrucao,
                          const char16_t *prompt,
                          char16_t *buffer,
                          size_t capacidade,
                          int permitir_vazio)
{
    for (;;)
    {
        AppLayout layout;
        tui16_input_result result;

        if (!app_screen(titulo, instrucao,
                        u"ENTER confirmar campo | ESC cancelar ação", &layout))
            return 0;

        result = tui16_input(layout.row + 8, layout.col + 4,
                             prompt, buffer, capacidade);
        if (result == TUI16_INPUT_CANCELLED)
            return 0;
        if (result == TUI16_INPUT_ERROR)
        {
            app_message(u"Falha de entrada",
                        u"Não foi possível editar o campo.",
                        u"A operação foi cancelada sem alterar os dados.");
            return 0;
        }
        if (permitir_vazio || buffer[0] != u'\0')
            return 1;

        app_message(u"Entrada inválida",
                    u"O campo não pode ficar vazio.",
                    u"Digite um valor ou pressione ESC para cancelar.");
    }
}

static int app_input_u32(const char16_t *titulo,
                         const char16_t *instrucao,
                         const char16_t *prompt,
                         uint32_t *valor,
                         int aceitar_zero)
{
    char16_t buffer[APP_NUM_CAP];

    if (valor == NULL)
        return 0;

    (void)app_u64_text(*valor, buffer, APP_NUM_CAP);

    for (;;)
    {
        uint32_t convertido;

        if (!app_input_text(titulo, instrucao, prompt,
                            buffer, APP_NUM_CAP, 0))
            return 0;

        if (app_parse_u32(buffer, &convertido) &&
            (aceitar_zero || convertido > 0U))
        {
            *valor = convertido;
            return 1;
        }

        app_message(u"Número inválido",
                    aceitar_zero
                        ? u"Informe um inteiro entre 0 e 4294967295."
                        : u"Informe um inteiro entre 1 e 4294967295.",
                    u"Letras, sinais e números fora do intervalo são rejeitados.");
    }
}

static int app_input_preco(const char16_t *titulo,
                           const char16_t *instrucao,
                           const char16_t *prompt,
                           double *valor)
{
    char16_t buffer[APP_NUM_CAP];

    if (valor == NULL)
        return 0;

    (void)app_double_text(*valor, buffer, APP_NUM_CAP);

    for (;;)
    {
        double convertido;

        if (!app_input_text(titulo, instrucao, prompt,
                            buffer, APP_NUM_CAP, 0))
            return 0;

        if (app_parse_double_positivo(buffer, &convertido))
        {
            *valor = convertido;
            return 1;
        }

        app_message(u"Preço inválido",
                    u"Informe um número positivo e finito.",
                    u"São aceitos ponto ou vírgula como separador decimal.");
    }
}

static void app_product_row(const Produto *produto,
                            char16_t *linha,
                            size_t capacidade)
{
    char16_t id[APP_NUM_CAP];
    char16_t preco[APP_NUM_CAP];
    char16_t quantidade[APP_NUM_CAP];

    linha[0] = u'\0';
    if (produto == NULL)
        return;

    (void)app_u64_text(produto->id, id, APP_NUM_CAP);
    (void)app_double_text(produto->preco, preco, APP_NUM_CAP);
    (void)app_u64_text(produto->quantidade, quantidade, APP_NUM_CAP);

    (void)app_append16(linha, capacidade, u"#");
    (void)app_append16(linha, capacidade, id);
    (void)app_append16(linha, capacidade, u" | cód: ");
    (void)app_append16(linha, capacidade, produto->codbar);
    (void)app_append16(linha, capacidade, u" | ");
    (void)app_append16(linha, capacidade, produto->nome);
    (void)app_append16(linha, capacidade, u" | R$ ");
    (void)app_append16(linha, capacidade, preco);
    (void)app_append16(linha, capacidade, u" | qtd: ");
    (void)app_append16(linha, capacidade, quantidade);
}

static void app_show_product(const char16_t *titulo, const Produto *produto)
{
    AppLayout layout;
    char16_t id[APP_NUM_CAP];
    char16_t preco[APP_NUM_CAP];
    char16_t quantidade[APP_NUM_CAP];

    if (produto == NULL)
    {
        app_message(u"Produto não encontrado",
                    u"Nenhum cadastro corresponde ao código informado.",
                    u"Revise o código e tente novamente.");
        return;
    }

    if (!app_screen(titulo, u"Detalhes do produto",
                    u"ENTER ou ESC: voltar", &layout))
        return;

    (void)app_u64_text(produto->id, id, APP_NUM_CAP);
    (void)app_double_text(produto->preco, preco, APP_NUM_CAP);
    (void)app_u64_text(produto->quantidade, quantidade, APP_NUM_CAP);

    app_draw_label_value(&layout, 6, u"ID interno: ", id);
    app_draw_label_value(&layout, 8, u"Código: ", produto->codbar);
    app_draw_label_value(&layout, 10, u"Nome: ", produto->nome);
    app_draw_label_value(&layout, 12, u"Categoria: ", produto->categoria);
    app_draw_label_value(&layout, 14, u"Preço: R$ ", preco);
    app_draw_label_value(&layout, 16, u"Quantidade: ", quantidade);

    (void)app_wait_key();
}

static void app_show_products(const char16_t *titulo,
                              const Produto *const *produtos,
                              size_t total)
{
    size_t pagina = 0U;
    size_t total_paginas;

    if (total == 0U)
    {
        app_message(titulo,
                    u"Nenhum produto foi encontrado.",
                    u"Use ESC ou ENTER para retornar.");
        return;
    }

    total_paginas = (total + APP_PAGE_SIZE - 1U) / APP_PAGE_SIZE;

    for (;;)
    {
        AppLayout layout;
        size_t inicio = pagina * APP_PAGE_SIZE;
        size_t fim = inicio + APP_PAGE_SIZE;
        char16_t pagina_texto[APP_TEXT_CAP];
        char16_t numero[APP_NUM_CAP];
        AppKey key;

        if (fim > total)
            fim = total;

        if (!app_screen(titulo,
                        u"Produtos encontrados",
                        u"← → páginas | ENTER ou ESC: voltar", &layout))
            return;

        pagina_texto[0] = u'\0';
        (void)app_append16(pagina_texto, APP_TEXT_CAP, u"Página ");
        (void)app_u64_text(pagina + 1U, numero, APP_NUM_CAP);
        (void)app_append16(pagina_texto, APP_TEXT_CAP, numero);
        (void)app_append16(pagina_texto, APP_TEXT_CAP, u" de ");
        (void)app_u64_text(total_paginas, numero, APP_NUM_CAP);
        (void)app_append16(pagina_texto, APP_TEXT_CAP, numero);
        tui16_drawtext(layout.row + 5, layout.col + 4, pagina_texto);

        for (size_t i = inicio; i < fim; ++i)
        {
            char16_t linha[APP_TEXT_CAP];
            app_product_row(produtos[i], linha, APP_TEXT_CAP);
            app_truncate16(linha, (size_t)(layout.width - 8));
            tui16_drawtext(layout.row + 7 + (int)((i - inicio) * 2U),
                           layout.col + 4, linha);
        }

        key = app_wait_key();
        if (key == APP_KEY_LEFT && pagina > 0U)
            --pagina;
        else if (key == APP_KEY_RIGHT && pagina + 1U < total_paginas)
            ++pagina;
        else if (key == APP_KEY_ENTER || key == APP_KEY_ESC)
            return;
    }
}

static const Produto **app_sorted_products(const Estoque *estoque, size_t *total)
{
    const Produto **produtos;
    size_t quantidade;

    if (total == NULL)
        return NULL;

    quantidade = estoque_total_produtos(estoque);
    *total = quantidade;
    if (quantidade == 0U)
        return NULL;

    produtos = malloc(quantidade * sizeof(*produtos));
    if (produtos == NULL)
        return NULL;

    for (size_t i = 0U; i < quantidade; ++i)
        produtos[i] = estoque_produto_ordenado(estoque, i);

    return produtos;
}

static void app_show_sorted(App *app)
{
    const Produto **produtos;
    size_t total;

    produtos = app_sorted_products(&app->estoque, &total);
    if (total > 0U && produtos == NULL)
    {
        app_message(u"Falha de memória",
                    u"Não foi possível montar a listagem.",
                    u"Nenhum dado foi alterado.");
        return;
    }

    app_show_products(u"Produtos por código", produtos, total);
    free(produtos);
}

static void app_persistence_result(const char16_t *operacao,
                                   ArquivoStatus status)
{
    char16_t detalhe[APP_TEXT_CAP];

    detalhe[0] = u'\0';
    (void)app_append16(detalhe, APP_TEXT_CAP, u"Persistência: ");
    (void)app_ascii_to16(arquivo_status_texto(status),
                         detalhe + app_length16(detalhe),
                         APP_TEXT_CAP - app_length16(detalhe));

    if (status == ARQUIVO_OK)
    {
        app_message(u"Operação concluída", operacao,
                    u"Os arquivos produtos.csv16 e estoque.csv16 estão atualizados.");
    }
    else
    {
        app_message(u"Atenção: falha ao salvar", operacao, detalhe);
    }
}

static void app_save_after_mutation(App *app, const char16_t *operacao)
{
    ArquivoStatus status = arquivo_salvar_estoque(&app->estoque,
                                                  APP_ARQUIVO_PRODUTOS,
                                                  APP_ARQUIVO_ESTOQUE);
    app_persistence_result(operacao, status);
}

static int app_collect_product(char16_t codbar[PRODUTO_CODBAR_CAP],
                               char16_t nome[PRODUTO_NOME_CAP],
                               double *preco,
                               uint32_t *quantidade,
                               char16_t categoria[PRODUTO_CATEGORIA_CAP],
                               const char16_t *titulo)
{
    return app_input_text(titulo, u"Informe o código único do produto.",
                          u"Código: ", codbar, PRODUTO_CODBAR_CAP, 0) &&
           app_input_text(titulo, u"Informe o nome do produto.",
                          u"Nome: ", nome, PRODUTO_NOME_CAP, 0) &&
           app_input_text(titulo, u"Informe a categoria do produto.",
                          u"Categoria: ", categoria, PRODUTO_CATEGORIA_CAP, 0) &&
           app_input_preco(titulo, u"Informe um preço positivo.",
                           u"Preço: R$ ", preco) &&
           app_input_u32(titulo, u"Informe a quantidade atual em estoque.",
                         u"Quantidade: ", quantidade, 1);
}

static void app_register_product(App *app)
{
    char16_t codbar[PRODUTO_CODBAR_CAP] = u"";
    char16_t nome[PRODUTO_NOME_CAP] = u"";
    char16_t categoria[PRODUTO_CATEGORIA_CAP] = u"";
    double preco = 1.0;
    uint32_t quantidade = 0U;
    Produto validacao;
    EstoqueStatus status;

    if (!app_collect_product(codbar, nome, &preco, &quantidade, categoria,
                             u"Cadastrar produto"))
        return;

    if (!produto_init(&validacao, 1U, codbar, nome, preco, quantidade, categoria))
    {
        app_message(u"Dados inválidos",
                    u"O produto não atende às regras de cadastro.",
                    u"Verifique texto vazio, tamanho dos campos e preço positivo.");
        return;
    }

    app_show_product(u"Prévia do cadastro", &validacao);
    if (!app_confirmar(u"Confirmar cadastro",
                       u"Deseja cadastrar este produto e salvar os arquivos?"))
        return;

    status = estoque_cadastrar_produto(&app->estoque, codbar, nome,
                                       preco, quantidade, categoria, NULL);
    if (status != ESTOQUE_OK)
    {
        char16_t detalhe[APP_TEXT_CAP];
        (void)app_ascii_to16(estoque_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Cadastro rejeitado",
                    u"O estoque não aceitou o novo produto.", detalhe);
        return;
    }

    app_save_after_mutation(app, u"Produto cadastrado com sucesso.");
}

static int app_ask_code(const char16_t *titulo,
                        const char16_t *instrucao,
                        char16_t codigo[PRODUTO_CODBAR_CAP])
{
    codigo[0] = u'\0';
    return app_input_text(titulo, instrucao, u"Código: ", codigo,
                          PRODUTO_CODBAR_CAP, 0);
}

static void app_search_code(App *app)
{
    char16_t codigo[PRODUTO_CODBAR_CAP];

    if (!app_ask_code(u"Buscar por código",
                      u"A consulta usa busca binária no vetor ordenado.", codigo))
        return;

    app_show_product(u"Resultado da busca binária",
                     estoque_buscar_por_codigo_const(&app->estoque, codigo));
}

static void app_search_name(App *app)
{
    char16_t nome[PRODUTO_NOME_CAP] = u"";
    const Produto **resultados;
    size_t total;

    if (!app_input_text(u"Buscar por nome",
                        u"A consulta usa busca linear por trecho exato do nome.",
                        u"Trecho: ", nome, PRODUTO_NOME_CAP, 0))
        return;

    total = estoque_buscar_por_nome(&app->estoque, nome, NULL, 0U);
    resultados = total == 0U ? NULL : malloc(total * sizeof(*resultados));
    if (total > 0U && resultados == NULL)
    {
        app_message(u"Falha de memória",
                    u"Não foi possível montar a listagem.",
                    u"Nenhum dado foi alterado.");
        return;
    }

    (void)estoque_buscar_por_nome(&app->estoque, nome, resultados, total);
    app_show_products(u"Produtos por nome", resultados, total);
    free(resultados);
}

static void app_edit_product(App *app)
{
    char16_t codigo_atual[PRODUTO_CODBAR_CAP];
    Produto original;
    Produto editado;
    Produto validacao;
    EstoqueStatus status;
    int codigo_reposicionado = 0;

    if (!app_ask_code(u"Editar produto",
                      u"Informe o código atual do produto.", codigo_atual))
        return;

    {
        const Produto *produto = estoque_buscar_por_codigo_const(&app->estoque,
                                                                 codigo_atual);
        if (produto == NULL)
        {
            app_show_product(u"Editar produto", NULL);
            return;
        }
        original = *produto;
        editado = original;
    }

    if (!app_collect_product(editado.codbar, editado.nome, &editado.preco,
                             &editado.quantidade, editado.categoria,
                             u"Editar produto"))
        return;

    if (!produto_init(&validacao, editado.id, editado.codbar, editado.nome,
                      editado.preco, editado.quantidade, editado.categoria))
    {
        app_message(u"Dados inválidos",
                    u"A edição não atende às regras do produto.",
                    u"Nenhuma alteração foi aplicada.");
        return;
    }

    app_show_product(u"Prévia da edição", &validacao);
    if (!app_confirmar(u"Confirmar edição",
                       u"Deseja aplicar a edição e salvar os arquivos?"))
        return;

    status = estoque_alterar_codigo(&app->estoque, codigo_atual,
                                    validacao.codbar);
    if (status == ESTOQUE_OK)
    {
        codigo_reposicionado = !str16_equal(codigo_atual, validacao.codbar);
        status = estoque_atualizar_produto(&app->estoque, validacao.codbar,
                                           validacao.nome, validacao.preco,
                                           validacao.quantidade,
                                           validacao.categoria);
    }

    if (status != ESTOQUE_OK)
    {
        char16_t detalhe[APP_TEXT_CAP];

        if (codigo_reposicionado)
        {
            (void)estoque_alterar_codigo(&app->estoque, validacao.codbar,
                                         original.codbar);
        }

        (void)app_ascii_to16(estoque_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Edição rejeitada",
                    u"O estoque não aceitou a alteração.", detalhe);
        return;
    }

    app_save_after_mutation(app, u"Produto atualizado com sucesso.");
}

static void app_remove_product(App *app)
{
    char16_t codigo[PRODUTO_CODBAR_CAP];
    const Produto *produto;
    EstoqueStatus status;

    if (!app_ask_code(u"Remover produto",
                      u"Informe o código do produto que será removido.", codigo))
        return;

    produto = estoque_buscar_por_codigo_const(&app->estoque, codigo);
    if (produto == NULL)
    {
        app_show_product(u"Remover produto", NULL);
        return;
    }

    app_show_product(u"Produto selecionado para remoção", produto);
    if (!app_confirmar(u"Confirmar remoção",
                       u"Deseja remover o produto? O histórico de vendas será preservado."))
        return;

    status = estoque_remover_produto(&app->estoque, codigo);
    if (status != ESTOQUE_OK)
    {
        char16_t detalhe[APP_TEXT_CAP];
        (void)app_ascii_to16(estoque_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Remoção rejeitada",
                    u"O produto não foi removido.", detalhe);
        return;
    }

    app_save_after_mutation(app, u"Produto removido com sucesso.");
}

static void app_register_sale(App *app)
{
    char16_t codigo[PRODUTO_CODBAR_CAP];
    const Produto *produto;
    uint32_t quantidade = 1U;
    EstoqueStatus status;
    Transacao transacao;

    if (!app_ask_code(u"Registrar venda",
                      u"Informe o código do produto vendido.", codigo))
        return;

    produto = estoque_buscar_por_codigo_const(&app->estoque, codigo);
    if (produto == NULL)
    {
        app_show_product(u"Registrar venda", NULL);
        return;
    }

    app_show_product(u"Produto selecionado para venda", produto);
    if (!app_input_u32(u"Registrar venda",
                       u"Informe a quantidade vendida. Zero não é permitido.",
                       u"Quantidade vendida: ", &quantidade, 0))
        return;

    if (!app_confirmar(u"Confirmar venda",
                       u"Deseja registrar a venda, reduzir o saldo e salvar os arquivos?"))
        return;

    status = estoque_registrar_venda(&app->estoque, codigo, quantidade, &transacao);
    if (status != ESTOQUE_OK)
    {
        char16_t detalhe[APP_TEXT_CAP];
        (void)app_ascii_to16(estoque_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Venda rejeitada",
                    u"A venda não foi registrada.", detalhe);
        return;
    }

    app_save_after_mutation(app, u"Venda registrada e saldo atualizado com sucesso.");
}

static void app_transaction_row(const Transacao *transacao,
                                char16_t *linha,
                                size_t capacidade)
{
    char16_t id[APP_NUM_CAP];
    char16_t quantidade[APP_NUM_CAP];
    char16_t preco[APP_NUM_CAP];
    char16_t momento[APP_NUM_CAP];

    linha[0] = u'\0';
    if (transacao == NULL)
        return;

    (void)app_u64_text(transacao->id, id, APP_NUM_CAP);
    (void)app_u64_text(transacao->quantidade, quantidade, APP_NUM_CAP);
    (void)app_double_text(transacao->preco_unitario, preco, APP_NUM_CAP);
    if (!app_time_text(transacao->registrada_em, momento, APP_NUM_CAP))
        app_copy16(momento, APP_NUM_CAP, u"data indisponível");

    (void)app_append16(linha, capacidade, u"Venda #");
    (void)app_append16(linha, capacidade, id);
    (void)app_append16(linha, capacidade, u" | cód: ");
    (void)app_append16(linha, capacidade, transacao->codigo_produto);
    (void)app_append16(linha, capacidade, u" | qtd: ");
    (void)app_append16(linha, capacidade, quantidade);
    (void)app_append16(linha, capacidade, u" | R$ ");
    (void)app_append16(linha, capacidade, preco);
    (void)app_append16(linha, capacidade, u" | ");
    (void)app_append16(linha, capacidade, momento);
}

static void app_show_transactions(App *app)
{
    size_t total = estoque_total_transacoes(&app->estoque);
    size_t pagina = 0U;
    size_t total_paginas;

    if (total == 0U)
    {
        app_message(u"Histórico de vendas",
                    u"Ainda não existem vendas registradas.",
                    u"O histórico será preenchido após a primeira venda.");
        return;
    }

    total_paginas = (total + APP_PAGE_SIZE - 1U) / APP_PAGE_SIZE;

    for (;;)
    {
        AppLayout layout;
        size_t inicio = pagina * APP_PAGE_SIZE;
        size_t fim = inicio + APP_PAGE_SIZE;
        AppKey key;

        if (fim > total)
            fim = total;

        if (!app_screen(u"Histórico de vendas",
                        u"Registros imutáveis das transações confirmadas.",
                        u"← → páginas | ENTER ou ESC: voltar", &layout))
            return;

        for (size_t i = inicio; i < fim; ++i)
        {
            char16_t linha[APP_TEXT_CAP];
            app_transaction_row(estoque_transacao(&app->estoque, i),
                                linha, APP_TEXT_CAP);
            app_truncate16(linha, (size_t)(layout.width - 8));
            tui16_drawtext(layout.row + 6 + (int)((i - inicio) * 2U),
                           layout.col + 4, linha);
        }

        key = app_wait_key();
        if (key == APP_KEY_LEFT && pagina > 0U)
            --pagina;
        else if (key == APP_KEY_RIGHT && pagina + 1U < total_paginas)
            ++pagina;
        else if (key == APP_KEY_ENTER || key == APP_KEY_ESC)
            return;
    }
}

static void app_report_category(App *app)
{
    char16_t categoria[PRODUTO_CATEGORIA_CAP] = u"";
    const Produto **resultados;
    size_t total;

    if (!app_input_text(u"Relatório por categoria",
                        u"Informe a categoria exata que deseja filtrar.",
                        u"Categoria: ", categoria, PRODUTO_CATEGORIA_CAP, 0))
        return;

    total = estoque_listar_por_categoria(&app->estoque, categoria, NULL, 0U);
    resultados = total == 0U ? NULL : malloc(total * sizeof(*resultados));
    if (total > 0U && resultados == NULL)
    {
        app_message(u"Falha de memória",
                    u"Não foi possível montar o relatório.",
                    u"Nenhum dado foi alterado.");
        return;
    }

    (void)estoque_listar_por_categoria(&app->estoque, categoria,
                                       resultados, total);
    app_show_products(u"Relatório por categoria", resultados, total);
    free(resultados);
}

static void app_report_low_stock(App *app)
{
    const Produto **resultados;
    size_t total;

    if (!app_input_u32(u"Relatório de estoque baixo",
                       u"Produtos com quantidade menor que o limite serão exibidos.",
                       u"Limite configurável: ", &app->limite_baixo, 1))
        return;

    total = estoque_listar_baixo_estoque(&app->estoque,
                                         app->limite_baixo, NULL, 0U);
    resultados = total == 0U ? NULL : malloc(total * sizeof(*resultados));
    if (total > 0U && resultados == NULL)
    {
        app_message(u"Falha de memória",
                    u"Não foi possível montar o relatório.",
                    u"Nenhum dado foi alterado.");
        return;
    }

    (void)estoque_listar_baixo_estoque(&app->estoque, app->limite_baixo,
                                       resultados, total);
    app_show_products(u"Produtos com estoque baixo", resultados, total);
    free(resultados);
}

static void app_report_price_extremes(App *app)
{
    const Produto *menor = estoque_produto_menor_preco(&app->estoque);
    const Produto *maior = estoque_produto_maior_preco(&app->estoque);
    AppLayout layout;
    char16_t linha[APP_TEXT_CAP];

    if (menor == NULL || maior == NULL)
    {
        app_message(u"Extremos de preço",
                    u"Não existem produtos cadastrados.",
                    u"Cadastre ao menos um produto para gerar este relatório.");
        return;
    }

    if (!app_screen(u"Extremos de preço",
                    u"Menor e maior preço encontrados por varredura linear.",
                    u"ENTER ou ESC: voltar", &layout))
        return;

    tui16_drawtext(layout.row + 6, layout.col + 4, u"Produto com menor preço:");
    app_product_row(menor, linha, APP_TEXT_CAP);
    app_truncate16(linha, (size_t)(layout.width - 10));
    tui16_drawtext(layout.row + 8, layout.col + 6, linha);

    tui16_drawtext(layout.row + 12, layout.col + 4, u"Produto com maior preço:");
    app_product_row(maior, linha, APP_TEXT_CAP);
    app_truncate16(linha, (size_t)(layout.width - 10));
    tui16_drawtext(layout.row + 14, layout.col + 6, linha);

    (void)app_wait_key();
}

static void app_save_now(App *app)
{
    ArquivoStatus status;

    if (!app_confirmar(u"Salvar dados",
                       u"Deseja gravar uma fotografia atual do estoque nos dois arquivos?"))
        return;

    status = arquivo_salvar_estoque(&app->estoque,
                                    APP_ARQUIVO_PRODUTOS,
                                    APP_ARQUIVO_ESTOQUE);
    app_persistence_result(u"Solicitação de salvamento finalizada.", status);
}

static void app_reload(App *app)
{
    ArquivoStatus status;

    if (!app_confirmar(u"Recarregar arquivos",
                       u"Deseja descartar o estado em memória e reler os dois arquivos?"))
        return;

    status = arquivo_carregar_estoque(&app->estoque,
                                      APP_ARQUIVO_PRODUTOS,
                                      APP_ARQUIVO_ESTOQUE);
    if (status == ARQUIVO_OK)
    {
        app_message(u"Dados recarregados",
                    u"O estoque em memória foi substituído com segurança.",
                    u"Produtos, saldos, IDs de controle e histórico foram restaurados.");
    }
    else
    {
        char16_t detalhe[APP_TEXT_CAP];
        (void)app_ascii_to16(arquivo_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Falha ao recarregar",
                    u"O estado anterior em memória foi preservado.", detalhe);
    }
}

static void app_products_menu(App *app)
{
    static const char16_t *const itens[] = {
        u"1. Cadastrar produto",
        u"2. Editar produto",
        u"3. Remover produto",
        u"4. Buscar produto por código",
        u"5. Buscar produtos por nome",
        u"6. Listar produtos por código",
        u"7. Voltar"};

    for (;;)
    {
        int opcao = app_menu(u"Produtos",
                             u"Cadastro, consultas e listagem ordenada.",
                             itens, 7, 0);
        if (opcao < 0 || opcao == 6)
            return;

        switch (opcao)
        {
        case 0:
            app_register_product(app);
            break;
        case 1:
            app_edit_product(app);
            break;
        case 2:
            app_remove_product(app);
            break;
        case 3:
            app_search_code(app);
            break;
        case 4:
            app_search_name(app);
            break;
        case 5:
            app_show_sorted(app);
            break;
        default:
            break;
        }
    }
}

static void app_sales_menu(App *app)
{
    static const char16_t *const itens[] = {
        u"1. Registrar venda",
        u"2. Consultar histórico de vendas",
        u"3. Voltar"};

    for (;;)
    {
        int opcao = app_menu(u"Vendas",
                             u"Baixa de saldo e histórico imutável.",
                             itens, 3, 0);
        if (opcao < 0 || opcao == 2)
            return;

        if (opcao == 0)
            app_register_sale(app);
        else if (opcao == 1)
            app_show_transactions(app);
    }
}

static void app_reports_menu(App *app)
{
    static const char16_t *const itens[] = {
        u"1. Produtos por categoria",
        u"2. Produtos com estoque baixo",
        u"3. Menor e maior preço",
        u"4. Voltar"};

    for (;;)
    {
        int opcao = app_menu(u"Relatórios",
                             u"Filtros e indicadores do cadastro atual.",
                             itens, 4, 0);
        if (opcao < 0 || opcao == 3)
            return;

        if (opcao == 0)
            app_report_category(app);
        else if (opcao == 1)
            app_report_low_stock(app);
        else if (opcao == 2)
            app_report_price_extremes(app);
    }
}

static void app_files_menu(App *app)
{
    static const char16_t *const itens[] = {
        u"1. Salvar agora",
        u"2. Recarregar arquivos",
        u"3. Voltar"};

    for (;;)
    {
        int opcao = app_menu(u"Persistência",
                             u"Controle explícito dos dois arquivos UTF-16LE.",
                             itens, 3, 0);
        if (opcao < 0 || opcao == 2)
            return;

        if (opcao == 0)
            app_save_now(app);
        else if (opcao == 1)
            app_reload(app);
    }
}

static void app_show_about(void)
{
    AppLayout layout;

    if (!app_screen(u"Sobre o sistema",
                    u"Projeto de Estrutura de Dados: estoque e vendas.",
                    u"ENTER ou ESC: voltar", &layout))
        return;

    tui16_drawtext(layout.row + 6, layout.col + 4,
                   u"• Cadastro com código único, categoria, preço e quantidade.");
    tui16_drawtext(layout.row + 8, layout.col + 4,
                   u"• Busca binária O(log n) em vetor ordenado por código.");
    tui16_drawtext(layout.row + 10, layout.col + 4,
                   u"• Busca linear O(n) por nome na ordem de cadastro.");
    tui16_drawtext(layout.row + 12, layout.col + 4,
                   u"• Venda com validação de saldo e histórico imutável.");
    tui16_drawtext(layout.row + 14, layout.col + 4,
                   u"• Persistência transacional por arquivos temporários e backups.");
    tui16_drawtext(layout.row + 18, layout.col + 4,
                   u"Profundidade máxima: menu principal → submenu → ação.");

    (void)app_wait_key();
}

static void app_initial_load(App *app)
{
    ArquivoStatus status = arquivo_carregar_estoque(&app->estoque,
                                                    APP_ARQUIVO_PRODUTOS,
                                                    APP_ARQUIVO_ESTOQUE);

    if (status == ARQUIVO_OK)
    {
        app_message(u"Dados carregados",
                    u"Os arquivos existentes foram carregados com sucesso.",
                    u"O sistema está pronto para uso.");
    }
    else if (status == ARQUIVO_ERRO_NAO_ENCONTRADO)
    {
        app_message(u"Novo estoque",
                    u"Os arquivos persistidos ainda não existem.",
                    u"O sistema iniciou vazio e criará os arquivos na primeira gravação.");
    }
    else
    {
        char16_t detalhe[APP_TEXT_CAP];
        (void)app_ascii_to16(arquivo_status_texto(status), detalhe, APP_TEXT_CAP);
        app_message(u"Falha ao carregar arquivos",
                    u"O sistema iniciou com estoque vazio para evitar dados parciais.",
                    detalhe);
    }
}

static int app_exit(App *app)
{
    static const char16_t *const itens[] = {
        u"1. Salvar e sair",
        u"2. Sair sem novo salvamento",
        u"3. Cancelar saída"};
    int opcao = app_menu(u"Encerrar aplicação",
                         u"Escolha como deseja finalizar a sessão.",
                         itens, 3, 2);

    if (opcao < 0 || opcao == 2)
        return 0;

    if (opcao == 0)
    {
        ArquivoStatus status = arquivo_salvar_estoque(&app->estoque,
                                                      APP_ARQUIVO_PRODUTOS,
                                                      APP_ARQUIVO_ESTOQUE);
        if (status != ARQUIVO_OK)
        {
            char16_t detalhe[APP_TEXT_CAP];
            (void)app_ascii_to16(arquivo_status_texto(status), detalhe, APP_TEXT_CAP);
            app_message(u"Falha ao salvar",
                        u"A aplicação continua aberta para você tentar novamente.",
                        detalhe);
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    static const char16_t *const itens[] = {
        u"1. Produtos",
        u"2. Vendas",
        u"3. Relatórios",
        u"4. Persistência",
        u"5. Sobre",
        u"6. Sair"};
    App app;
    int sair = 0;

    estoque_init(&app.estoque);
    app.limite_baixo = 5U;

    if (!tui16_init())
    {
        fputs("Nao foi possivel inicializar o terminal Windows.\n", stderr);
        estoque_destroy(&app.estoque);
        return EXIT_FAILURE;
    }

    app_initial_load(&app);

    while (!sair)
    {
        int opcao = app_menu(u"CONTROLE DE ESTOQUE",
                             u"Selecione uma área. Toda ação permite confirmação ou cancelamento.",
                             itens, 6, 0);

        if (opcao < 0 || opcao == 5)
        {
            sair = app_exit(&app);
            continue;
        }

        switch (opcao)
        {
        case 0:
            app_products_menu(&app);
            break;
        case 1:
            app_sales_menu(&app);
            break;
        case 2:
            app_reports_menu(&app);
            break;
        case 3:
            app_files_menu(&app);
            break;
        case 4:
            app_show_about();
            break;
        default:
            break;
        }
    }

    estoque_destroy(&app.estoque);
    tui16_shutdown();
    return EXIT_SUCCESS;
}
