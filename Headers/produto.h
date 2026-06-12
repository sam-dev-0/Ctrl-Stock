#ifndef PRODUTO_H
#define PRODUTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

/*
 * Cada capacidade inclui o terminador nulo UTF-16.
 * Portanto, PRODUTO_CODBAR_CAP == 16 comporta até 15 unidades UTF-16 úteis.
 */
#define PRODUTO_CODBAR_CAP   16U
#define PRODUTO_NOME_CAP     64U
#define PRODUTO_CATEGORIA_CAP 64U

typedef struct Produto
{
    uint32_t id; /* Identificador interno estável, gerado pelo estoque. */
    char16_t codbar[PRODUTO_CODBAR_CAP]; /* Código de negócio único. */
    char16_t nome[PRODUTO_NOME_CAP];
    double preco;
    uint32_t quantidade;
    char16_t categoria[PRODUTO_CATEGORIA_CAP];
} Produto;

/*
 * Inicializa um produto somente quando todos os dados são válidos.
 * Em caso de erro, o objeto apontado por produto não é alterado.
 */
bool produto_init(Produto *produto,
                  uint32_t id,
                  const char16_t *codbar,
                  const char16_t *nome,
                  double preco,
                  uint32_t quantidade,
                  const char16_t *categoria);

/* Valida integralmente um produto já existente. */
bool produto_validar(const Produto *produto);

/* Setters com validação local. */
bool produto_setcodbar(Produto *produto, const char16_t *codbar);
bool produto_setnome(Produto *produto, const char16_t *nome);
bool produto_setpreco(Produto *produto, double preco);
bool produto_setquantidade(Produto *produto, uint32_t quantidade);
bool produto_setcategoria(Produto *produto, const char16_t *categoria);

#endif
