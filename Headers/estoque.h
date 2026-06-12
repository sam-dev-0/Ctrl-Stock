#ifndef ESTOQUE_H
#define ESTOQUE_H

#include "produto.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef enum EstoqueStatus
{
    ESTOQUE_OK = 0,
    ESTOQUE_ERRO_ARGUMENTO,
    ESTOQUE_ERRO_MEMORIA,
    ESTOQUE_ERRO_CAPACIDADE,
    ESTOQUE_ERRO_DADOS_INVALIDOS,
    ESTOQUE_ERRO_CODIGO_DUPLICADO,
    ESTOQUE_ERRO_ID_DUPLICADO,
    ESTOQUE_ERRO_PRODUTO_NAO_ENCONTRADO,
    ESTOQUE_ERRO_QUANTIDADE_INVALIDA,
    ESTOQUE_ERRO_ESTOQUE_INSUFICIENTE,
    ESTOQUE_ERRO_TRANSACAO_INVALIDA,
    ESTOQUE_ERRO_TRANSACAO_DUPLICADA
} EstoqueStatus;

typedef struct Transacao
{
    uint64_t id;
    uint32_t produto_id;
    char16_t codigo_produto[PRODUTO_CODBAR_CAP];
    uint32_t quantidade;
    double preco_unitario;
    time_t registrada_em;
} Transacao;

/*
 * O estoque possui duas visões dos mesmos objetos Produto:
 *
 * 1. por_codigo: vetor de ponteiros ordenado por codbar;
 *    permite busca binária O(log n).
 *
 * 2. cadastro: vetor de ponteiros na ordem de inserção;
 *    permite busca linear por nome O(n), sem duplicar produtos.
 */
typedef struct Estoque
{
    Produto **por_codigo;
    Produto **cadastro;
    size_t tamanho;
    size_t capacidade;
    uint32_t proximo_id;

    Transacao *transacoes;
    size_t total_transacoes;
    size_t capacidade_transacoes;
    uint64_t proxima_transacao_id;
} Estoque;

void estoque_init(Estoque *estoque);
void estoque_destroy(Estoque *estoque);

/* Cadastro comum: o estoque gera um ID interno sequencial. */
EstoqueStatus estoque_cadastrar_produto(Estoque *estoque,
                                        const char16_t *codbar,
                                        const char16_t *nome,
                                        double preco,
                                        uint32_t quantidade,
                                        const char16_t *categoria,
                                        uint32_t *id_gerado);

/*
 * Insere uma cópia de produto já existente.
 * Útil durante o carregamento de dados persistidos.
 */
EstoqueStatus estoque_inserir_produto(Estoque *estoque,
                                      const Produto *produto);

EstoqueStatus estoque_atualizar_produto(Estoque *estoque,
                                        const char16_t *codbar,
                                        const char16_t *nome,
                                        double preco,
                                        uint32_t quantidade,
                                        const char16_t *categoria);

/*
 * Alterar o código exige operação própria porque o vetor por_codigo
 * precisa ser reposicionado para continuar ordenado.
 */
EstoqueStatus estoque_alterar_codigo(Estoque *estoque,
                                     const char16_t *codigo_atual,
                                     const char16_t *novo_codigo);

EstoqueStatus estoque_remover_produto(Estoque *estoque,
                                      const char16_t *codbar);

Produto *estoque_buscar_por_codigo(Estoque *estoque,
                                   const char16_t *codbar);
const Produto *estoque_buscar_por_codigo_const(const Estoque *estoque,
                                               const char16_t *codbar);

/*
 * Retorna o total de correspondências encontradas.
 * Quando saida não é NULL, grava no máximo capacidade_saida ponteiros.
 */
size_t estoque_buscar_por_nome(const Estoque *estoque,
                               const char16_t *trecho_nome,
                               const Produto **saida,
                               size_t capacidade_saida);

size_t estoque_listar_por_categoria(const Estoque *estoque,
                                    const char16_t *categoria,
                                    const Produto **saida,
                                    size_t capacidade_saida);

size_t estoque_listar_baixo_estoque(const Estoque *estoque,
                                    uint32_t limite,
                                    const Produto **saida,
                                    size_t capacidade_saida);

const Produto *estoque_produto_menor_preco(const Estoque *estoque);
const Produto *estoque_produto_maior_preco(const Estoque *estoque);

EstoqueStatus estoque_registrar_venda(Estoque *estoque,
                                      const char16_t *codbar,
                                      uint32_t quantidade,
                                      Transacao *transacao_gerada);

/*
 * Insere uma transação histórica já persistida sem alterar o saldo atual.
 * Útil exclusivamente durante o carregamento dos arquivos.
 */
EstoqueStatus estoque_inserir_transacao(Estoque *estoque,
                                        const Transacao *transacao);

/*
 * Restaura os contadores persistidos depois de carregar produtos e transações.
 * O valor zero representa esgotamento do espaço de IDs.
 */
EstoqueStatus estoque_restaurar_controle(Estoque *estoque,
                                         uint32_t proximo_produto_id,
                                         uint64_t proxima_transacao_id);

size_t estoque_total_produtos(const Estoque *estoque);
const Produto *estoque_produto_ordenado(const Estoque *estoque,
                                        size_t indice);
const Produto *estoque_produto_cadastrado(const Estoque *estoque,
                                          size_t indice);
size_t estoque_total_transacoes(const Estoque *estoque);
const Transacao *estoque_transacao(const Estoque *estoque, size_t indice);

const char *estoque_status_texto(EstoqueStatus status);

#endif
