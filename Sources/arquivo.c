#include "arquivo.h"

#include "arq16_csv.h"
#include "str16.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DBL_DECIMAL_DIG
#define DBL_DECIMAL_DIG 17
#endif

#define ARQUIVO_DELIMITADOR u';'
#define ARQUIVO_TEXTO_NUM_CAP 64U

#define ARQUIVO_PRODUTOS_COLUNAS 6U
#define ARQUIVO_ESTOQUE_COLUNAS 9U

static const char16_t *const ARQUIVO_PRODUTOS_ROTULOS[ARQUIVO_PRODUTOS_COLUNAS] = {
    u"id",
    u"codigo",
    u"nome",
    u"categoria",
    u"preco",
    u"quantidade"
};

static const uint16_t ARQUIVO_PRODUTOS_LARGURAS[ARQUIVO_PRODUTOS_COLUNAS] = {
    10U, /* uint32_t */
    PRODUTO_CODBAR_CAP - 1U,
    PRODUTO_NOME_CAP - 1U,
    PRODUTO_CATEGORIA_CAP - 1U,
    32U, /* representação decimal de double */
    10U  /* uint32_t */
};

/*
 * O segundo arquivo guarda dois tipos de registro:
 * - controle: preserva os próximos IDs mesmo depois de exclusões;
 * - transacao: preserva cada venda histórica.
 */
static const char16_t *const ARQUIVO_ESTOQUE_ROTULOS[ARQUIVO_ESTOQUE_COLUNAS] = {
    u"tipo",
    u"id",
    u"produto_id",
    u"codigo_produto",
    u"quantidade",
    u"preco_unitario",
    u"registrada_em",
    u"proximo_produto_id",
    u"proxima_transacao_id"
};

static const uint16_t ARQUIVO_ESTOQUE_LARGURAS[ARQUIVO_ESTOQUE_COLUNAS] = {
    10U, /* tipo de registro */
    20U, /* id da transação: uint64_t */
    10U, /* produto_id: uint32_t */
    PRODUTO_CODBAR_CAP - 1U,
    10U, /* quantidade: uint32_t */
    32U, /* preço unitário: double */
    20U, /* timestamp não negativo */
    10U, /* próximo produto_id: uint32_t */
    20U  /* próxima transação_id: uint64_t */
};

typedef struct ArquivoCargaContexto
{
    Estoque *estoque;
    ArquivoStatus status;
    int controle_lido;
    uint32_t proximo_produto_id;
    uint64_t proxima_transacao_id;
} ArquivoCargaContexto;

static int arquivo_caminhos_validos(const char *produtos,
                                    const char *estoque)
{
    return produtos != NULL && estoque != NULL &&
           produtos[0] != '\0' && estoque[0] != '\0' &&
           strcmp(produtos, estoque) != 0;
}

static char *arquivo_com_sufixo(const char *caminho, const char *sufixo)
{
    char *resultado;
    size_t caminho_len;
    size_t sufixo_len;

    if (caminho == NULL || sufixo == NULL)
        return NULL;

    caminho_len = strlen(caminho);
    sufixo_len = strlen(sufixo);

    if (caminho_len > SIZE_MAX - sufixo_len - 1U)
        return NULL;

    resultado = malloc(caminho_len + sufixo_len + 1U);
    if (resultado == NULL)
        return NULL;

    memcpy(resultado, caminho, caminho_len);
    memcpy(resultado + caminho_len, sufixo, sufixo_len + 1U);
    return resultado;
}

static int arquivo_existe(const char *caminho)
{
    FILE *arquivo;

    arquivo = fopen(caminho, "rb");
    if (arquivo == NULL)
        return 0;

    fclose(arquivo);
    return 1;
}

static int arquivo_texto_u64(const char16_t *texto, uint64_t *valor)
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
        if (acumulado > (UINT64_MAX - digito) / UINT64_C(10))
            return 0;

        acumulado = acumulado * UINT64_C(10) + digito;
        ++i;
    }

    *valor = acumulado;
    return 1;
}

static int arquivo_texto_u32(const char16_t *texto, uint32_t *valor)
{
    uint64_t temporario;

    if (!arquivo_texto_u64(texto, &temporario) || temporario > UINT32_MAX)
        return 0;

    *valor = (uint32_t)temporario;
    return 1;
}

static int arquivo_u64_texto(uint64_t valor, char16_t *saida, size_t capacidade)
{
    char16_t reverso[32];
    size_t usados = 0U;
    size_t i;

    if (saida == NULL || capacidade == 0U)
        return 0;

    do
    {
        reverso[usados++] = (char16_t)(u'0' + (char16_t)(valor % UINT64_C(10)));
        valor /= UINT64_C(10);
    } while (valor != 0U);

    if (usados + 1U > capacidade)
    {
        saida[0] = u'\0';
        return 0;
    }

    for (i = 0U; i < usados; ++i)
        saida[i] = reverso[usados - i - 1U];

    saida[usados] = u'\0';
    return 1;
}

static int arquivo_texto_double(const char16_t *texto, double *valor)
{
    char16_t *fim;
    double temporario;

    if (texto == NULL || valor == NULL || texto[0] == u'\0')
        return 0;

    temporario = str16_forDouble(texto, &fim);
    if (fim == texto || fim == NULL || *fim != u'\0' ||
        !isfinite(temporario) || temporario <= 0.0)
    {
        return 0;
    }

    *valor = temporario;
    return 1;
}

static int arquivo_double_texto(double valor, char16_t *saida, size_t capacidade)
{
    char texto_ascii[ARQUIVO_TEXTO_NUM_CAP];
    int total;
    size_t i;

    if (saida == NULL || capacidade == 0U || !isfinite(valor) || valor <= 0.0)
        return 0;

    total = snprintf(texto_ascii, sizeof(texto_ascii), "%.*g",
                     DBL_DECIMAL_DIG, valor);
    if (total < 0 || (size_t)total >= sizeof(texto_ascii) ||
        (size_t)total + 1U > capacidade)
    {
        saida[0] = u'\0';
        return 0;
    }

    for (i = 0U; i < (size_t)total; ++i)
    {
        /* Mantém um formato persistido estável mesmo sob locale com vírgula. */
        saida[i] = texto_ascii[i] == ',' ? u'.' : (char16_t)(unsigned char)texto_ascii[i];
    }

    saida[total] = u'\0';
    return 1;
}

static int arquivo_texto_time(const char16_t *texto, time_t *valor)
{
    uint64_t temporario;
    time_t convertido;

    if (!arquivo_texto_u64(texto, &temporario))
        return 0;

    convertido = (time_t)temporario;
    if ((uint64_t)convertido != temporario)
        return 0;

    *valor = convertido;
    return 1;
}

static int arquivo_time_texto(time_t valor, char16_t *saida, size_t capacidade)
{
    uint64_t convertido;

    if (valor < (time_t)0)
        return 0;

    convertido = (uint64_t)valor;
    if ((time_t)convertido != valor)
        return 0;

    return arquivo_u64_texto(convertido, saida, capacidade);
}

static int arquivo_copiar_texto16(char16_t *destino,
                                  size_t capacidade,
                                  const char16_t *origem)
{
    size_t i;

    if (destino == NULL || origem == NULL || capacidade == 0U)
        return 0;

    for (i = 0U; i < capacidade; ++i)
    {
        destino[i] = origem[i];
        if (origem[i] == u'\0')
            return 1;
    }

    destino[capacidade - 1U] = u'\0';
    return 0;
}

static int arquivo_texto_vazio(const char16_t *texto)
{
    return texto != NULL && texto[0] == u'\0';
}

static int arquivo_schema_igual(const Header *header,
                                uint16_t total_colunas,
                                const uint16_t *larguras,
                                const char16_t *const *rotulos)
{
    uint16_t i;

    if (header == NULL || larguras == NULL || rotulos == NULL ||
        header->totalcolumns != total_colunas ||
        header->delimiter != ARQUIVO_DELIMITADOR)
    {
        return 0;
    }

    for (i = 0U; i < total_colunas; ++i)
    {
        if (header->sizecolumn[i] != larguras[i] ||
            !str16_equal(header->labels[i], rotulos[i]))
        {
            return 0;
        }
    }

    return 1;
}

static ArquivoStatus arquivo_status_estoque(EstoqueStatus status)
{
    switch (status)
    {
        case ESTOQUE_OK:
            return ARQUIVO_OK;
        case ESTOQUE_ERRO_MEMORIA:
        case ESTOQUE_ERRO_CAPACIDADE:
            return ARQUIVO_ERRO_MEMORIA;
        case ESTOQUE_ERRO_DADOS_INVALIDOS:
        case ESTOQUE_ERRO_CODIGO_DUPLICADO:
        case ESTOQUE_ERRO_ID_DUPLICADO:
        case ESTOQUE_ERRO_TRANSACAO_INVALIDA:
        case ESTOQUE_ERRO_TRANSACAO_DUPLICADA:
            return ARQUIVO_ERRO_DADOS_INVALIDOS;
        default:
            return ARQUIVO_ERRO_ESTOQUE;
    }
}

static int arquivo_csv16_inserir_linha(Header *header,
                                       const char16_t *const *valores)
{
    Changes *alteracoes;

    alteracoes = csv16_insert(header, NULL, valores);
    if (alteracoes == NULL)
        return 0;

    alteracoes = csv16_save(header, alteracoes);
    if (alteracoes != NULL)
    {
        csv16_free_changes(alteracoes);
        return 0;
    }

    return 1;
}

static ArquivoStatus arquivo_gravar_produtos(const Estoque *estoque,
                                             const char *caminho)
{
    Header *header;
    size_t total;
    size_t i;

    remove(caminho);
    header = csv16_create(caminho, u"produtos", ARQUIVO_DELIMITADOR,
                          ARQUIVO_PRODUTOS_COLUNAS,
                          ARQUIVO_PRODUTOS_LARGURAS,
                          ARQUIVO_PRODUTOS_ROTULOS);
    if (header == NULL)
        return ARQUIVO_ERRO_ABERTURA;

    total = estoque_total_produtos(estoque);
    for (i = 0U; i < total; ++i)
    {
        const Produto *produto = estoque_produto_ordenado(estoque, i);
        char16_t id[ARQUIVO_TEXTO_NUM_CAP];
        char16_t preco[ARQUIVO_TEXTO_NUM_CAP];
        char16_t quantidade[ARQUIVO_TEXTO_NUM_CAP];
        const char16_t *valores[ARQUIVO_PRODUTOS_COLUNAS];

        if (produto == NULL ||
            !arquivo_u64_texto(produto->id, id, ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_double_texto(produto->preco, preco, ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_u64_texto(produto->quantidade, quantidade,
                               ARQUIVO_TEXTO_NUM_CAP))
        {
            csv16_close(header);
            remove(caminho);
            return ARQUIVO_ERRO_DADOS_INVALIDOS;
        }

        valores[0] = id;
        valores[1] = produto->codbar;
        valores[2] = produto->nome;
        valores[3] = produto->categoria;
        valores[4] = preco;
        valores[5] = quantidade;

        if (!arquivo_csv16_inserir_linha(header, valores))
        {
            csv16_close(header);
            remove(caminho);
            return ARQUIVO_ERRO_ESCRITA;
        }
    }

    csv16_close(header);
    return ARQUIVO_OK;
}

static ArquivoStatus arquivo_gravar_controle_transacoes(const Estoque *estoque,
                                                        const char *caminho)
{
    Header *header;
    size_t total;
    size_t i;
    char16_t proximo_produto_id[ARQUIVO_TEXTO_NUM_CAP];
    char16_t proxima_transacao_id[ARQUIVO_TEXTO_NUM_CAP];
    const char16_t *controle[ARQUIVO_ESTOQUE_COLUNAS];

    remove(caminho);
    header = csv16_create(caminho, u"estoque", ARQUIVO_DELIMITADOR,
                          ARQUIVO_ESTOQUE_COLUNAS,
                          ARQUIVO_ESTOQUE_LARGURAS,
                          ARQUIVO_ESTOQUE_ROTULOS);
    if (header == NULL)
        return ARQUIVO_ERRO_ABERTURA;

    if (!arquivo_u64_texto(estoque->proximo_id, proximo_produto_id,
                           ARQUIVO_TEXTO_NUM_CAP) ||
        !arquivo_u64_texto(estoque->proxima_transacao_id,
                           proxima_transacao_id, ARQUIVO_TEXTO_NUM_CAP))
    {
        csv16_close(header);
        remove(caminho);
        return ARQUIVO_ERRO_DADOS_INVALIDOS;
    }

    controle[0] = u"controle";
    controle[1] = u"";
    controle[2] = u"";
    controle[3] = u"";
    controle[4] = u"";
    controle[5] = u"";
    controle[6] = u"";
    controle[7] = proximo_produto_id;
    controle[8] = proxima_transacao_id;

    if (!arquivo_csv16_inserir_linha(header, controle))
    {
        csv16_close(header);
        remove(caminho);
        return ARQUIVO_ERRO_ESCRITA;
    }

    total = estoque_total_transacoes(estoque);
    for (i = 0U; i < total; ++i)
    {
        const Transacao *transacao = estoque_transacao(estoque, i);
        char16_t id[ARQUIVO_TEXTO_NUM_CAP];
        char16_t produto_id[ARQUIVO_TEXTO_NUM_CAP];
        char16_t quantidade[ARQUIVO_TEXTO_NUM_CAP];
        char16_t preco[ARQUIVO_TEXTO_NUM_CAP];
        char16_t registrada_em[ARQUIVO_TEXTO_NUM_CAP];
        const char16_t *valores[ARQUIVO_ESTOQUE_COLUNAS];

        if (transacao == NULL ||
            !arquivo_u64_texto(transacao->id, id, ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_u64_texto(transacao->produto_id, produto_id,
                               ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_u64_texto(transacao->quantidade, quantidade,
                               ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_double_texto(transacao->preco_unitario, preco,
                                  ARQUIVO_TEXTO_NUM_CAP) ||
            !arquivo_time_texto(transacao->registrada_em, registrada_em,
                                ARQUIVO_TEXTO_NUM_CAP))
        {
            csv16_close(header);
            remove(caminho);
            return ARQUIVO_ERRO_DADOS_INVALIDOS;
        }

        valores[0] = u"transacao";
        valores[1] = id;
        valores[2] = produto_id;
        valores[3] = transacao->codigo_produto;
        valores[4] = quantidade;
        valores[5] = preco;
        valores[6] = registrada_em;
        valores[7] = u"";
        valores[8] = u"";

        if (!arquivo_csv16_inserir_linha(header, valores))
        {
            csv16_close(header);
            remove(caminho);
            return ARQUIVO_ERRO_ESCRITA;
        }
    }

    csv16_close(header);
    return ARQUIVO_OK;
}

static void arquivo_restaurar_backup(const char *principal,
                                     const char *backup,
                                     int possuia_backup)
{
    remove(principal);
    if (possuia_backup)
        rename(backup, principal);
}

static int arquivo_substituir_dupla(const char *produtos_tmp,
                                    const char *produtos,
                                    const char *estoque_tmp,
                                    const char *estoque)
{
    char *produtos_bak = NULL;
    char *estoque_bak = NULL;
    int havia_produtos = 0;
    int havia_estoque = 0;
    int produtos_instalados = 0;
    int estoque_instalado = 0;
    int ok = 0;

    produtos_bak = arquivo_com_sufixo(produtos, ".bak");
    estoque_bak = arquivo_com_sufixo(estoque, ".bak");
    if (produtos_bak == NULL || estoque_bak == NULL)
        goto fim;

    remove(produtos_bak);
    remove(estoque_bak);

    havia_produtos = arquivo_existe(produtos);
    havia_estoque = arquivo_existe(estoque);

    if (havia_produtos && rename(produtos, produtos_bak) != 0)
        goto fim;

    if (havia_estoque && rename(estoque, estoque_bak) != 0)
    {
        if (havia_produtos)
            rename(produtos_bak, produtos);
        goto fim;
    }

    if (rename(produtos_tmp, produtos) != 0)
        goto restaurar;
    produtos_instalados = 1;

    if (rename(estoque_tmp, estoque) != 0)
        goto restaurar;
    estoque_instalado = 1;

    remove(produtos_bak);
    remove(estoque_bak);
    ok = 1;
    goto fim;

restaurar:
    if (produtos_instalados)
        remove(produtos);
    if (estoque_instalado)
        remove(estoque);

    arquivo_restaurar_backup(produtos, produtos_bak, havia_produtos);
    arquivo_restaurar_backup(estoque, estoque_bak, havia_estoque);

fim:
    free(produtos_bak);
    free(estoque_bak);
    return ok;
}

ArquivoStatus arquivo_salvar_estoque(const Estoque *estoque,
                                     const char *arquivo_produtos,
                                     const char *arquivo_estoque)
{
    char *produtos_tmp;
    char *estoque_tmp;
    ArquivoStatus status;

    if (estoque == NULL ||
        !arquivo_caminhos_validos(arquivo_produtos, arquivo_estoque))
    {
        return ARQUIVO_ERRO_ARGUMENTO;
    }

    produtos_tmp = arquivo_com_sufixo(arquivo_produtos, ".tmp");
    estoque_tmp = arquivo_com_sufixo(arquivo_estoque, ".tmp");
    if (produtos_tmp == NULL || estoque_tmp == NULL)
    {
        free(produtos_tmp);
        free(estoque_tmp);
        return ARQUIVO_ERRO_MEMORIA;
    }

    remove(produtos_tmp);
    remove(estoque_tmp);

    status = arquivo_gravar_produtos(estoque, produtos_tmp);
    if (status != ARQUIVO_OK)
        goto fim;

    status = arquivo_gravar_controle_transacoes(estoque, estoque_tmp);
    if (status != ARQUIVO_OK)
        goto fim;

    if (!arquivo_substituir_dupla(produtos_tmp, arquivo_produtos,
                                  estoque_tmp, arquivo_estoque))
    {
        status = ARQUIVO_ERRO_SUBSTITUICAO;
        goto fim;
    }

    status = ARQUIVO_OK;

fim:
    remove(produtos_tmp);
    remove(estoque_tmp);
    free(produtos_tmp);
    free(estoque_tmp);
    return status;
}

static int arquivo_visitar_produto(int64_t linha,
                                   const char16_t *const *valores,
                                   uint16_t total_colunas,
                                   void *contexto_ptr)
{
    ArquivoCargaContexto *contexto = contexto_ptr;
    Produto produto;
    uint32_t id;
    uint32_t quantidade;
    double preco;
    EstoqueStatus status;

    (void)linha;

    if (contexto == NULL || valores == NULL ||
        total_colunas != ARQUIVO_PRODUTOS_COLUNAS ||
        !arquivo_texto_u32(valores[0], &id) || id == 0U ||
        !arquivo_texto_double(valores[4], &preco) ||
        !arquivo_texto_u32(valores[5], &quantidade) ||
        !produto_init(&produto, id, valores[1], valores[2], preco,
                      quantidade, valores[3]))
    {
        if (contexto != NULL)
            contexto->status = ARQUIVO_ERRO_DADOS_INVALIDOS;
        return 0;
    }

    status = estoque_inserir_produto(contexto->estoque, &produto);
    contexto->status = arquivo_status_estoque(status);
    return contexto->status == ARQUIVO_OK;
}

static int arquivo_visitar_estoque(int64_t linha,
                                    const char16_t *const *valores,
                                    uint16_t total_colunas,
                                    void *contexto_ptr)
{
    ArquivoCargaContexto *contexto = contexto_ptr;
    Transacao transacao;
    EstoqueStatus status;

    (void)linha;

    if (contexto == NULL || valores == NULL ||
        total_colunas != ARQUIVO_ESTOQUE_COLUNAS)
    {
        if (contexto != NULL)
            contexto->status = ARQUIVO_ERRO_DADOS_INVALIDOS;
        return 0;
    }

    if (str16_equal(valores[0], u"controle"))
    {
        if (contexto->controle_lido ||
            !arquivo_texto_vazio(valores[1]) ||
            !arquivo_texto_vazio(valores[2]) ||
            !arquivo_texto_vazio(valores[3]) ||
            !arquivo_texto_vazio(valores[4]) ||
            !arquivo_texto_vazio(valores[5]) ||
            !arquivo_texto_vazio(valores[6]) ||
            !arquivo_texto_u32(valores[7], &contexto->proximo_produto_id) ||
            !arquivo_texto_u64(valores[8], &contexto->proxima_transacao_id))
        {
            contexto->status = ARQUIVO_ERRO_DADOS_INVALIDOS;
            return 0;
        }

        contexto->controle_lido = 1;
        return 1;
    }

    if (!str16_equal(valores[0], u"transacao"))
    {
        contexto->status = ARQUIVO_ERRO_DADOS_INVALIDOS;
        return 0;
    }

    memset(&transacao, 0, sizeof(transacao));
    if (!arquivo_texto_vazio(valores[7]) ||
        !arquivo_texto_vazio(valores[8]) ||
        !arquivo_texto_u64(valores[1], &transacao.id) || transacao.id == 0U ||
        !arquivo_texto_u32(valores[2], &transacao.produto_id) ||
        transacao.produto_id == 0U ||
        !arquivo_copiar_texto16(transacao.codigo_produto,
                                PRODUTO_CODBAR_CAP, valores[3]) ||
        !arquivo_texto_u32(valores[4], &transacao.quantidade) ||
        transacao.quantidade == 0U ||
        !arquivo_texto_double(valores[5], &transacao.preco_unitario) ||
        !arquivo_texto_time(valores[6], &transacao.registrada_em))
    {
        contexto->status = ARQUIVO_ERRO_DADOS_INVALIDOS;
        return 0;
    }

    status = estoque_inserir_transacao(contexto->estoque, &transacao);
    contexto->status = arquivo_status_estoque(status);
    return contexto->status == ARQUIVO_OK;
}

static ArquivoStatus arquivo_carregar_produtos(Estoque *estoque,
                                               const char *caminho)
{
    Header *header;
    ArquivoCargaContexto contexto;
    int ok;

    if (!arquivo_existe(caminho))
        return ARQUIVO_ERRO_NAO_ENCONTRADO;

    header = csv16_open(caminho, u"produtos", ARQUIVO_DELIMITADOR);
    if (header == NULL)
        return ARQUIVO_ERRO_FORMATO;

    if (!arquivo_schema_igual(header, ARQUIVO_PRODUTOS_COLUNAS,
                              ARQUIVO_PRODUTOS_LARGURAS,
                              ARQUIVO_PRODUTOS_ROTULOS))
    {
        csv16_close(header);
        return ARQUIVO_ERRO_FORMATO;
    }

    memset(&contexto, 0, sizeof(contexto));
    contexto.estoque = estoque;
    contexto.status = ARQUIVO_OK;
    ok = csv16_foreach(header, arquivo_visitar_produto, &contexto);
    csv16_close(header);

    if (!ok && contexto.status == ARQUIVO_OK)
        return ARQUIVO_ERRO_FORMATO;

    return contexto.status;
}

static ArquivoStatus arquivo_carregar_controle_transacoes(Estoque *estoque,
                                                          const char *caminho)
{
    Header *header;
    ArquivoCargaContexto contexto;
    EstoqueStatus estoque_status;
    int ok;

    if (!arquivo_existe(caminho))
        return ARQUIVO_ERRO_NAO_ENCONTRADO;

    header = csv16_open(caminho, u"estoque", ARQUIVO_DELIMITADOR);
    if (header == NULL)
        return ARQUIVO_ERRO_FORMATO;

    if (!arquivo_schema_igual(header, ARQUIVO_ESTOQUE_COLUNAS,
                              ARQUIVO_ESTOQUE_LARGURAS,
                              ARQUIVO_ESTOQUE_ROTULOS))
    {
        csv16_close(header);
        return ARQUIVO_ERRO_FORMATO;
    }

    memset(&contexto, 0, sizeof(contexto));
    contexto.estoque = estoque;
    contexto.status = ARQUIVO_OK;
    ok = csv16_foreach(header, arquivo_visitar_estoque, &contexto);
    csv16_close(header);

    if (!ok && contexto.status == ARQUIVO_OK)
        return ARQUIVO_ERRO_FORMATO;

    if (contexto.status != ARQUIVO_OK)
        return contexto.status;

    if (!contexto.controle_lido)
        return ARQUIVO_ERRO_DADOS_INVALIDOS;

    estoque_status = estoque_restaurar_controle(estoque,
                                                contexto.proximo_produto_id,
                                                contexto.proxima_transacao_id);
    return arquivo_status_estoque(estoque_status);
}

ArquivoStatus arquivo_carregar_estoque(Estoque *estoque,
                                       const char *arquivo_produtos,
                                       const char *arquivo_estoque)
{
    Estoque temporario;
    ArquivoStatus status;

    if (estoque == NULL ||
        !arquivo_caminhos_validos(arquivo_produtos, arquivo_estoque))
    {
        return ARQUIVO_ERRO_ARGUMENTO;
    }

    estoque_init(&temporario);

    status = arquivo_carregar_produtos(&temporario, arquivo_produtos);
    if (status != ARQUIVO_OK)
        goto falha;

    status = arquivo_carregar_controle_transacoes(&temporario, arquivo_estoque);
    if (status != ARQUIVO_OK)
        goto falha;

    estoque_destroy(estoque);
    *estoque = temporario;
    return ARQUIVO_OK;

falha:
    estoque_destroy(&temporario);
    return status;
}

const char *arquivo_status_texto(ArquivoStatus status)
{
    switch (status)
    {
        case ARQUIVO_OK:
            return "operacao concluida";
        case ARQUIVO_ERRO_ARGUMENTO:
            return "argumento invalido";
        case ARQUIVO_ERRO_MEMORIA:
            return "memoria insuficiente";
        case ARQUIVO_ERRO_NAO_ENCONTRADO:
            return "arquivo nao encontrado";
        case ARQUIVO_ERRO_ABERTURA:
            return "nao foi possivel criar ou abrir o arquivo";
        case ARQUIVO_ERRO_FORMATO:
            return "arquivo ausente, corrompido ou com formato inesperado";
        case ARQUIVO_ERRO_DADOS_INVALIDOS:
            return "arquivo contem dados invalidos ou duplicados";
        case ARQUIVO_ERRO_ESTOQUE:
            return "estoque rejeitou os dados carregados";
        case ARQUIVO_ERRO_ESCRITA:
            return "falha ao gravar os registros";
        case ARQUIVO_ERRO_SUBSTITUICAO:
            return "falha ao substituir os arquivos persistidos";
        default:
            return "status desconhecido";
    }
}
