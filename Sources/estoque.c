#include "estoque.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ESTOQUE_CAPACIDADE_INICIAL 8U
#define TRANSACOES_CAPACIDADE_INICIAL 8U

static int texto16_comparar(const char16_t *a, const char16_t *b)
{
    size_t i = 0U;

    if (a == b)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;

    while (a[i] != u'\0' && b[i] != u'\0')
    {
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
        ++i;
    }

    if (a[i] == b[i])
        return 0;

    return a[i] == u'\0' ? -1 : 1;
}

static bool texto16_contem(const char16_t *texto, const char16_t *trecho)
{
    size_t inicio;
    size_t deslocamento;

    if (texto == NULL || trecho == NULL || trecho[0] == u'\0')
        return false;

    for (inicio = 0U; texto[inicio] != u'\0'; ++inicio)
    {
        deslocamento = 0U;
        while (trecho[deslocamento] != u'\0' &&
               texto[inicio + deslocamento] == trecho[deslocamento])
        {
            ++deslocamento;
        }

        if (trecho[deslocamento] == u'\0')
            return true;
    }

    return false;
}

static void texto16_copiar_codigo(char16_t destino[PRODUTO_CODBAR_CAP],
                                  const char16_t origem[PRODUTO_CODBAR_CAP])
{
    size_t i;

    for (i = 0U; i < PRODUTO_CODBAR_CAP; ++i)
    {
        destino[i] = origem[i];
        if (origem[i] == u'\0')
            return;
    }

    /* Defesa extra: um Produto válido já deve conter terminador. */
    destino[PRODUTO_CODBAR_CAP - 1U] = u'\0';
}

static bool texto16_codigo_valido(const char16_t codigo[PRODUTO_CODBAR_CAP])
{
    bool possui_conteudo = false;
    size_t i;

    if (codigo == NULL)
        return false;

    for (i = 0U; i < PRODUTO_CODBAR_CAP; ++i)
    {
        if (codigo[i] == u'\0')
            return possui_conteudo;

        if (codigo[i] != u' ' && codigo[i] != u'\t' &&
            codigo[i] != u'\n' && codigo[i] != u'\r' &&
            codigo[i] != u'\v' && codigo[i] != u'\f')
        {
            possui_conteudo = true;
        }
    }

    return false;
}

static bool estoque_transacao_id_existe(const Estoque *estoque, uint64_t id)
{
    size_t i;

    for (i = 0U; i < estoque->total_transacoes; ++i)
    {
        if (estoque->transacoes[i].id == id)
            return true;
    }

    return false;
}

static bool estoque_transacao_valida(const Transacao *transacao)
{
    return transacao != NULL &&
           transacao->id != 0U &&
           transacao->produto_id != 0U &&
           texto16_codigo_valido(transacao->codigo_produto) &&
           transacao->quantidade != 0U &&
           isfinite(transacao->preco_unitario) &&
           transacao->preco_unitario > 0.0 &&
           transacao->registrada_em >= (time_t)0;
}

/*
 * Retorna a posição do código, quando encontrado, ou a posição correta
 * de inserção para preservar a ordenação. Essa é a busca binária central.
 */
static size_t estoque_posicao_codigo(const Estoque *estoque,
                                     const char16_t *codbar,
                                     bool *encontrado)
{
    size_t inicio = 0U;
    size_t fim = estoque->tamanho;

    while (inicio < fim)
    {
        size_t meio = inicio + (fim - inicio) / 2U;
        int comparacao = texto16_comparar(estoque->por_codigo[meio]->codbar,
                                          codbar);

        if (comparacao < 0)
            inicio = meio + 1U;
        else
            fim = meio;
    }

    if (encontrado != NULL)
    {
        *encontrado = inicio < estoque->tamanho &&
                      texto16_comparar(estoque->por_codigo[inicio]->codbar,
                                       codbar) == 0;
    }

    return inicio;
}

static bool estoque_id_existe(const Estoque *estoque, uint32_t id)
{
    size_t i;

    for (i = 0U; i < estoque->tamanho; ++i)
    {
        if (estoque->cadastro[i]->id == id)
            return true;
    }

    return false;
}

static size_t estoque_indice_cadastro(const Estoque *estoque,
                                      const Produto *produto)
{
    size_t i;

    for (i = 0U; i < estoque->tamanho; ++i)
    {
        if (estoque->cadastro[i] == produto)
            return i;
    }

    return estoque->tamanho;
}

/*
 * Cresce os dois vetores de produtos de forma atômica: se uma alocação
 * falhar, o estoque original permanece utilizável e sem vazamentos.
 */
static EstoqueStatus estoque_reservar_produtos(Estoque *estoque,
                                               size_t minimo)
{
    Produto **novo_por_codigo;
    Produto **novo_cadastro;
    size_t nova_capacidade;

    if (minimo <= estoque->capacidade)
        return ESTOQUE_OK;

    nova_capacidade = estoque->capacidade == 0U
                          ? ESTOQUE_CAPACIDADE_INICIAL
                          : estoque->capacidade;

    while (nova_capacidade < minimo)
    {
        if (nova_capacidade > SIZE_MAX / 2U)
            return ESTOQUE_ERRO_CAPACIDADE;
        nova_capacidade *= 2U;
    }

    if (nova_capacidade > SIZE_MAX / sizeof(*novo_por_codigo))
        return ESTOQUE_ERRO_CAPACIDADE;

    novo_por_codigo = malloc(nova_capacidade * sizeof(*novo_por_codigo));
    novo_cadastro = malloc(nova_capacidade * sizeof(*novo_cadastro));

    if (novo_por_codigo == NULL || novo_cadastro == NULL)
    {
        free(novo_por_codigo);
        free(novo_cadastro);
        return ESTOQUE_ERRO_MEMORIA;
    }

    if (estoque->tamanho > 0U)
    {
        memcpy(novo_por_codigo,
               estoque->por_codigo,
               estoque->tamanho * sizeof(*novo_por_codigo));
        memcpy(novo_cadastro,
               estoque->cadastro,
               estoque->tamanho * sizeof(*novo_cadastro));
    }

    free(estoque->por_codigo);
    free(estoque->cadastro);
    estoque->por_codigo = novo_por_codigo;
    estoque->cadastro = novo_cadastro;
    estoque->capacidade = nova_capacidade;

    return ESTOQUE_OK;
}

static EstoqueStatus estoque_reservar_transacoes(Estoque *estoque,
                                                 size_t minimo)
{
    Transacao *novas_transacoes;
    size_t nova_capacidade;

    if (minimo <= estoque->capacidade_transacoes)
        return ESTOQUE_OK;

    nova_capacidade = estoque->capacidade_transacoes == 0U
                          ? TRANSACOES_CAPACIDADE_INICIAL
                          : estoque->capacidade_transacoes;

    while (nova_capacidade < minimo)
    {
        if (nova_capacidade > SIZE_MAX / 2U)
            return ESTOQUE_ERRO_CAPACIDADE;
        nova_capacidade *= 2U;
    }

    if (nova_capacidade > SIZE_MAX / sizeof(*novas_transacoes))
        return ESTOQUE_ERRO_CAPACIDADE;

    novas_transacoes = realloc(estoque->transacoes,
                               nova_capacidade * sizeof(*novas_transacoes));

    if (novas_transacoes == NULL)
        return ESTOQUE_ERRO_MEMORIA;

    estoque->transacoes = novas_transacoes;
    estoque->capacidade_transacoes = nova_capacidade;
    return ESTOQUE_OK;
}

static EstoqueStatus estoque_inserir_copia(Estoque *estoque,
                                           const Produto *produto)
{
    Produto *copia;
    EstoqueStatus status;
    bool encontrado;
    size_t posicao;

    if (estoque == NULL || !produto_validar(produto))
        return ESTOQUE_ERRO_DADOS_INVALIDOS;

    posicao = estoque_posicao_codigo(estoque, produto->codbar, &encontrado);
    if (encontrado)
        return ESTOQUE_ERRO_CODIGO_DUPLICADO;

    if (estoque_id_existe(estoque, produto->id))
        return ESTOQUE_ERRO_ID_DUPLICADO;

    if (estoque->tamanho == SIZE_MAX)
        return ESTOQUE_ERRO_CAPACIDADE;

    status = estoque_reservar_produtos(estoque, estoque->tamanho + 1U);
    if (status != ESTOQUE_OK)
        return status;

    copia = malloc(sizeof(*copia));
    if (copia == NULL)
        return ESTOQUE_ERRO_MEMORIA;

    *copia = *produto;

    memmove(&estoque->por_codigo[posicao + 1U],
            &estoque->por_codigo[posicao],
            (estoque->tamanho - posicao) * sizeof(*estoque->por_codigo));

    estoque->por_codigo[posicao] = copia;
    estoque->cadastro[estoque->tamanho] = copia;
    ++estoque->tamanho;

    if (copia->id == UINT32_MAX)
    {
        estoque->proximo_id = 0U; /* Sinaliza esgotamento dos IDs. */
    }
    else if (estoque->proximo_id != 0U && copia->id >= estoque->proximo_id)
    {
        estoque->proximo_id = copia->id + 1U;
    }

    return ESTOQUE_OK;
}

void estoque_init(Estoque *estoque)
{
    if (estoque == NULL)
        return;

    memset(estoque, 0, sizeof(*estoque));
    estoque->proximo_id = 1U;
    estoque->proxima_transacao_id = 1U;
}

void estoque_destroy(Estoque *estoque)
{
    size_t i;

    if (estoque == NULL)
        return;

    for (i = 0U; i < estoque->tamanho; ++i)
        free(estoque->cadastro[i]);

    free(estoque->por_codigo);
    free(estoque->cadastro);
    free(estoque->transacoes);
    memset(estoque, 0, sizeof(*estoque));
}

EstoqueStatus estoque_cadastrar_produto(Estoque *estoque,
                                        const char16_t *codbar,
                                        const char16_t *nome,
                                        double preco,
                                        uint32_t quantidade,
                                        const char16_t *categoria,
                                        uint32_t *id_gerado)
{
    Produto produto;
    EstoqueStatus status;

    if (estoque == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    if (estoque->proximo_id == 0U)
        return ESTOQUE_ERRO_CAPACIDADE;

    if (!produto_init(&produto,
                      estoque->proximo_id,
                      codbar,
                      nome,
                      preco,
                      quantidade,
                      categoria))
    {
        return ESTOQUE_ERRO_DADOS_INVALIDOS;
    }

    status = estoque_inserir_copia(estoque, &produto);
    if (status == ESTOQUE_OK && id_gerado != NULL)
        *id_gerado = produto.id;

    return status;
}

EstoqueStatus estoque_inserir_produto(Estoque *estoque,
                                      const Produto *produto)
{
    if (estoque == NULL || produto == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    return estoque_inserir_copia(estoque, produto);
}

Produto *estoque_buscar_por_codigo(Estoque *estoque,
                                   const char16_t *codbar)
{
    bool encontrado;
    size_t posicao;

    if (estoque == NULL || codbar == NULL || codbar[0] == u'\0')
        return NULL;

    posicao = estoque_posicao_codigo(estoque, codbar, &encontrado);
    return encontrado ? estoque->por_codigo[posicao] : NULL;
}

const Produto *estoque_buscar_por_codigo_const(const Estoque *estoque,
                                               const char16_t *codbar)
{
    bool encontrado;
    size_t posicao;

    if (estoque == NULL || codbar == NULL || codbar[0] == u'\0')
        return NULL;

    posicao = estoque_posicao_codigo(estoque, codbar, &encontrado);
    return encontrado ? estoque->por_codigo[posicao] : NULL;
}

EstoqueStatus estoque_atualizar_produto(Estoque *estoque,
                                        const char16_t *codbar,
                                        const char16_t *nome,
                                        double preco,
                                        uint32_t quantidade,
                                        const char16_t *categoria)
{
    Produto *produto;
    Produto temporario;

    if (estoque == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    produto = estoque_buscar_por_codigo(estoque, codbar);
    if (produto == NULL)
        return ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO;

    temporario = *produto;
    if (!produto_setnome(&temporario, nome) ||
        !produto_setpreco(&temporario, preco) ||
        !produto_setquantidade(&temporario, quantidade) ||
        !produto_setcategoria(&temporario, categoria))
    {
        return ESTOQUE_ERRO_DADOS_INVALIDOS;
    }

    *produto = temporario;
    return ESTOQUE_OK;
}

EstoqueStatus estoque_alterar_codigo(Estoque *estoque,
                                     const char16_t *codigo_atual,
                                     const char16_t *novo_codigo)
{
    Produto *produto;
    Produto temporario;
    bool encontrado;
    size_t posicao_atual;
    size_t nova_posicao;

    if (estoque == NULL || codigo_atual == NULL || novo_codigo == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    posicao_atual = estoque_posicao_codigo(estoque,
                                           codigo_atual,
                                           &encontrado);
    if (!encontrado)
        return ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO;

    produto = estoque->por_codigo[posicao_atual];
    temporario = *produto;

    if (!produto_setcodbar(&temporario, novo_codigo))
        return ESTOQUE_ERRO_DADOS_INVALIDOS;

    nova_posicao = estoque_posicao_codigo(estoque,
                                          temporario.codbar,
                                          &encontrado);
    if (encontrado && estoque->por_codigo[nova_posicao] != produto)
        return ESTOQUE_ERRO_CODIGO_DUPLICADO;

    if (texto16_comparar(codigo_atual, novo_codigo) == 0)
        return ESTOQUE_OK;

    /* Remove temporariamente o ponteiro do índice ordenado. */
    memmove(&estoque->por_codigo[posicao_atual],
            &estoque->por_codigo[posicao_atual + 1U],
            (estoque->tamanho - posicao_atual - 1U) *
                sizeof(*estoque->por_codigo));
    --estoque->tamanho;

    *produto = temporario;

    nova_posicao = estoque_posicao_codigo(estoque, produto->codbar, NULL);
    memmove(&estoque->por_codigo[nova_posicao + 1U],
            &estoque->por_codigo[nova_posicao],
            (estoque->tamanho - nova_posicao) * sizeof(*estoque->por_codigo));
    estoque->por_codigo[nova_posicao] = produto;
    ++estoque->tamanho;

    return ESTOQUE_OK;
}

EstoqueStatus estoque_remover_produto(Estoque *estoque,
                                      const char16_t *codbar)
{
    Produto *produto;
    bool encontrado;
    size_t indice_codigo;
    size_t indice_cadastro;

    if (estoque == NULL || codbar == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    indice_codigo = estoque_posicao_codigo(estoque, codbar, &encontrado);
    if (!encontrado)
        return ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO;

    produto = estoque->por_codigo[indice_codigo];
    indice_cadastro = estoque_indice_cadastro(estoque, produto);

    if (indice_cadastro == estoque->tamanho)
        return ESTOQUE_ERRO_DADOS_INVALIDOS;

    memmove(&estoque->por_codigo[indice_codigo],
            &estoque->por_codigo[indice_codigo + 1U],
            (estoque->tamanho - indice_codigo - 1U) *
                sizeof(*estoque->por_codigo));

    memmove(&estoque->cadastro[indice_cadastro],
            &estoque->cadastro[indice_cadastro + 1U],
            (estoque->tamanho - indice_cadastro - 1U) *
                sizeof(*estoque->cadastro));

    free(produto);
    --estoque->tamanho;
    estoque->por_codigo[estoque->tamanho] = NULL;
    estoque->cadastro[estoque->tamanho] = NULL;

    return ESTOQUE_OK;
}

size_t estoque_buscar_por_nome(const Estoque *estoque,
                               const char16_t *trecho_nome,
                               const Produto **saida,
                               size_t capacidade_saida)
{
    size_t i;
    size_t total = 0U;

    if (estoque == NULL || trecho_nome == NULL || trecho_nome[0] == u'\0')
        return 0U;

    for (i = 0U; i < estoque->tamanho; ++i)
    {
        if (texto16_contem(estoque->cadastro[i]->nome, trecho_nome))
        {
            if (saida != NULL && total < capacidade_saida)
                saida[total] = estoque->cadastro[i];
            ++total;
        }
    }

    return total;
}

size_t estoque_listar_por_categoria(const Estoque *estoque,
                                    const char16_t *categoria,
                                    const Produto **saida,
                                    size_t capacidade_saida)
{
    size_t i;
    size_t total = 0U;

    if (estoque == NULL || categoria == NULL || categoria[0] == u'\0')
        return 0U;

    for (i = 0U; i < estoque->tamanho; ++i)
    {
        if (texto16_comparar(estoque->cadastro[i]->categoria, categoria) == 0)
        {
            if (saida != NULL && total < capacidade_saida)
                saida[total] = estoque->cadastro[i];
            ++total;
        }
    }

    return total;
}

size_t estoque_listar_baixo_estoque(const Estoque *estoque,
                                    uint32_t limite,
                                    const Produto **saida,
                                    size_t capacidade_saida)
{
    size_t i;
    size_t total = 0U;

    if (estoque == NULL)
        return 0U;

    for (i = 0U; i < estoque->tamanho; ++i)
    {
        if (estoque->cadastro[i]->quantidade < limite)
        {
            if (saida != NULL && total < capacidade_saida)
                saida[total] = estoque->cadastro[i];
            ++total;
        }
    }

    return total;
}

const Produto *estoque_produto_menor_preco(const Estoque *estoque)
{
    const Produto *menor;
    size_t i;

    if (estoque == NULL || estoque->tamanho == 0U)
        return NULL;

    menor = estoque->cadastro[0];
    for (i = 1U; i < estoque->tamanho; ++i)
    {
        if (estoque->cadastro[i]->preco < menor->preco)
            menor = estoque->cadastro[i];
    }

    return menor;
}

const Produto *estoque_produto_maior_preco(const Estoque *estoque)
{
    const Produto *maior;
    size_t i;

    if (estoque == NULL || estoque->tamanho == 0U)
        return NULL;

    maior = estoque->cadastro[0];
    for (i = 1U; i < estoque->tamanho; ++i)
    {
        if (estoque->cadastro[i]->preco > maior->preco)
            maior = estoque->cadastro[i];
    }

    return maior;
}

EstoqueStatus estoque_registrar_venda(Estoque *estoque,
                                      const char16_t *codbar,
                                      uint32_t quantidade,
                                      Transacao *transacao_gerada)
{
    Produto *produto;
    Transacao transacao;
    EstoqueStatus status;

    if (estoque == NULL || codbar == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    if (quantidade == 0U)
        return ESTOQUE_ERRO_QUANTIDADE_INVALIDA;

    produto = estoque_buscar_por_codigo(estoque, codbar);
    if (produto == NULL)
        return ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO;

    if (produto->quantidade < quantidade)
        return ESTOQUE_ERRO_ESTOQUE_INSUFICIENTE;

    if (estoque->proxima_transacao_id == 0U)
        return ESTOQUE_ERRO_CAPACIDADE;

    if (estoque->total_transacoes == SIZE_MAX)
        return ESTOQUE_ERRO_CAPACIDADE;

    /* Reserva antes de alterar o saldo: uma falha não cria venda parcial. */
    status = estoque_reservar_transacoes(estoque,
                                         estoque->total_transacoes + 1U);
    if (status != ESTOQUE_OK)
        return status;

    memset(&transacao, 0, sizeof(transacao));
    transacao.id = estoque->proxima_transacao_id;
    transacao.produto_id = produto->id;
    texto16_copiar_codigo(transacao.codigo_produto, produto->codbar);
    transacao.quantidade = quantidade;
    transacao.preco_unitario = produto->preco;
    transacao.registrada_em = time(NULL);

    produto->quantidade -= quantidade;
    estoque->transacoes[estoque->total_transacoes] = transacao;
    ++estoque->total_transacoes;

    if (estoque->proxima_transacao_id == UINT64_MAX)
        estoque->proxima_transacao_id = 0U;
    else
        ++estoque->proxima_transacao_id;

    if (transacao_gerada != NULL)
        *transacao_gerada = transacao;

    return ESTOQUE_OK;
}

EstoqueStatus estoque_inserir_transacao(Estoque *estoque,
                                        const Transacao *transacao)
{
    EstoqueStatus status;

    if (estoque == NULL || transacao == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    if (!estoque_transacao_valida(transacao))
        return ESTOQUE_ERRO_TRANSACAO_INVALIDA;

    if (estoque_transacao_id_existe(estoque, transacao->id))
        return ESTOQUE_ERRO_TRANSACAO_DUPLICADA;

    if (estoque->total_transacoes == SIZE_MAX)
        return ESTOQUE_ERRO_CAPACIDADE;

    status = estoque_reservar_transacoes(estoque,
                                         estoque->total_transacoes + 1U);
    if (status != ESTOQUE_OK)
        return status;

    estoque->transacoes[estoque->total_transacoes] = *transacao;
    ++estoque->total_transacoes;

    if (transacao->id == UINT64_MAX)
    {
        estoque->proxima_transacao_id = 0U;
    }
    else if (estoque->proxima_transacao_id != 0U &&
             transacao->id >= estoque->proxima_transacao_id)
    {
        estoque->proxima_transacao_id = transacao->id + 1U;
    }

    return ESTOQUE_OK;
}

EstoqueStatus estoque_restaurar_controle(Estoque *estoque,
                                         uint32_t proximo_produto_id,
                                         uint64_t proxima_transacao_id)
{
    size_t i;

    if (estoque == NULL)
        return ESTOQUE_ERRO_ARGUMENTO;

    if (proximo_produto_id != 0U)
    {
        for (i = 0U; i < estoque->tamanho; ++i)
        {
            if (estoque->cadastro[i]->id >= proximo_produto_id)
                return ESTOQUE_ERRO_DADOS_INVALIDOS;
        }

        for (i = 0U; i < estoque->total_transacoes; ++i)
        {
            if (estoque->transacoes[i].produto_id >= proximo_produto_id)
                return ESTOQUE_ERRO_DADOS_INVALIDOS;
        }
    }

    if (proxima_transacao_id != 0U)
    {
        for (i = 0U; i < estoque->total_transacoes; ++i)
        {
            if (estoque->transacoes[i].id >= proxima_transacao_id)
                return ESTOQUE_ERRO_DADOS_INVALIDOS;
        }
    }

    estoque->proximo_id = proximo_produto_id;
    estoque->proxima_transacao_id = proxima_transacao_id;
    return ESTOQUE_OK;
}

size_t estoque_total_produtos(const Estoque *estoque)
{
    return estoque == NULL ? 0U : estoque->tamanho;
}

const Produto *estoque_produto_ordenado(const Estoque *estoque,
                                        size_t indice)
{
    if (estoque == NULL || indice >= estoque->tamanho)
        return NULL;

    return estoque->por_codigo[indice];
}

const Produto *estoque_produto_cadastrado(const Estoque *estoque,
                                          size_t indice)
{
    if (estoque == NULL || indice >= estoque->tamanho)
        return NULL;

    return estoque->cadastro[indice];
}

size_t estoque_total_transacoes(const Estoque *estoque)
{
    return estoque == NULL ? 0U : estoque->total_transacoes;
}

const Transacao *estoque_transacao(const Estoque *estoque, size_t indice)
{
    if (estoque == NULL || indice >= estoque->total_transacoes)
        return NULL;

    return &estoque->transacoes[indice];
}

const char *estoque_status_texto(EstoqueStatus status)
{
    switch (status)
    {
        case ESTOQUE_OK:
            return "operacao concluida";
        case ESTOQUE_ERRO_ARGUMENTO:
            return "argumento invalido";
        case ESTOQUE_ERRO_MEMORIA:
            return "memoria insuficiente";
        case ESTOQUE_ERRO_CAPACIDADE:
            return "limite de capacidade atingido";
        case ESTOQUE_ERRO_DADOS_INVALIDOS:
            return "dados do produto invalidos";
        case ESTOQUE_ERRO_CODIGO_DUPLICADO:
            return "codigo de produto duplicado";
        case ESTOQUE_ERRO_ID_DUPLICADO:
            return "identificador interno duplicado";
        case ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO:
            return "produto nao encontrado";
        case ESTOQUE_ERRO_QUANTIDADE_INVALIDA:
            return "quantidade de venda invalida";
        case ESTOQUE_ERRO_ESTOQUE_INSUFICIENTE:
            return "estoque insuficiente";
        case ESTOQUE_ERRO_TRANSACAO_INVALIDA:
            return "dados da transacao invalidos";
        case ESTOQUE_ERRO_TRANSACAO_DUPLICADA:
            return "identificador de transacao duplicado";
        default:
            return "status desconhecido";
    }
}
