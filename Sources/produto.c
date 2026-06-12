#include "produto.h"

#include <math.h>
#include <string.h>

static bool texto16_eh_espaco(char16_t caractere)
{
    return caractere == u' '  || caractere == u'\t' ||
           caractere == u'\n' || caractere == u'\r' ||
           caractere == u'\v' || caractere == u'\f';
}

/*
 * Aceita somente textos:
 * - não nulos;
 * - terminados dentro da capacidade de destino;
 * - com pelo menos um caractere diferente de espaço em branco.
 *
 * Isso impede leitura fora dos limites e evita campos apenas com espaços.
 */
static bool texto16_valido(const char16_t *texto, size_t capacidade)
{
    bool possui_conteudo = false;
    size_t i;

    if (texto == NULL || capacidade == 0U)
        return false;

    for (i = 0U; i < capacidade; ++i)
    {
        if (texto[i] == u'\0')
            return possui_conteudo;

        if (!texto16_eh_espaco(texto[i]))
            possui_conteudo = true;
    }

    /* Não houve terminador nulo dentro do limite permitido. */
    return false;
}

static bool texto16_copiar(char16_t *destino,
                           size_t capacidade,
                           const char16_t *origem)
{
    size_t i;

    if (destino == NULL || !texto16_valido(origem, capacidade))
        return false;

    for (i = 0U; i < capacidade; ++i)
    {
        destino[i] = origem[i];
        if (origem[i] == u'\0')
            return true;
    }

    return false;
}

bool produto_init(Produto *produto,
                  uint32_t id,
                  const char16_t *codbar,
                  const char16_t *nome,
                  double preco,
                  uint32_t quantidade,
                  const char16_t *categoria)
{
    Produto temporario;

    if (produto == NULL || id == 0U)
        return false;

    memset(&temporario, 0, sizeof(temporario));
    temporario.id = id;

    if (!produto_setcodbar(&temporario, codbar) ||
        !produto_setnome(&temporario, nome) ||
        !produto_setpreco(&temporario, preco) ||
        !produto_setquantidade(&temporario, quantidade) ||
        !produto_setcategoria(&temporario, categoria))
    {
        return false;
    }

    *produto = temporario;
    return true;
}

bool produto_validar(const Produto *produto)
{
    if (produto == NULL || produto->id == 0U)
        return false;

    return texto16_valido(produto->codbar, PRODUTO_CODBAR_CAP) &&
           texto16_valido(produto->nome, PRODUTO_NOME_CAP) &&
           isfinite(produto->preco) && produto->preco > 0.0 &&
           texto16_valido(produto->categoria, PRODUTO_CATEGORIA_CAP);
}

bool produto_setcodbar(Produto *produto, const char16_t *codbar)
{
    if (produto == NULL)
        return false;

    return texto16_copiar(produto->codbar, PRODUTO_CODBAR_CAP, codbar);
}

bool produto_setnome(Produto *produto, const char16_t *nome)
{
    if (produto == NULL)
        return false;

    return texto16_copiar(produto->nome, PRODUTO_NOME_CAP, nome);
}

bool produto_setpreco(Produto *produto, double preco)
{
    if (produto == NULL || !isfinite(preco) || preco <= 0.0)
        return false;

    produto->preco = preco;
    return true;
}

bool produto_setquantidade(Produto *produto, uint32_t quantidade)
{
    if (produto == NULL)
        return false;

    /* uint32_t já exclui quantidades negativas após uma entrada bem validada. */
    produto->quantidade = quantidade;
    return true;
}

bool produto_setcategoria(Produto *produto, const char16_t *categoria)
{
    if (produto == NULL)
        return false;

    return texto16_copiar(produto->categoria,
                          PRODUTO_CATEGORIA_CAP,
                          categoria);
}
