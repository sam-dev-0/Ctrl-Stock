#ifndef PRODUTO_H
#define PRODUTO_H

#include "str16.h"

typedef struct produto
{
    uint32_t id;
    char16_t codbar[16];
    char16_t nome[64];
    float preco;
    uint32_t qt;
    char16_t categoria[64];

} Produto;

Produto produto_cadastrar(char16_t *codbar, char16_t *nome, float preco, uint32_t qt, char16_t *categoria);

int produto_setcodbar(Produto *p, const char16_t *codbar);

int produto_setnome(Produto *p, const char16_t *nome);

int produto_setpreco(Produto *p, float preco);

int produto_setqt(Produto *p, uint32_t qt);

int produto_setcategoria(Produto *p, const char16_t *categoria);

#endif
