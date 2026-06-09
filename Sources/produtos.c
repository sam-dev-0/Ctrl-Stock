#include "produto.h"

Produto produto_cadastrar(char16_t *codbar, char16_t *nome, float preco, uint32_t qt, char16_t *categoria)
{
    Produto p;

    p.id = (rand() % 9000) + 1000;
    
    str16_copy(codbar, p.codbar);
    str16_copy(nome, p.nome);
    
    p.preco = preco;
    p.qt = qt;
    
    str16_copy(categoria, p.categoria);

    return p;
}

int produto_setcodbar(Produto *p, const char16_t *codbar)
{
    if (codbar == NULL || codbar[0] == u'\0')
        return 0;

    str16_copy(codbar, p->codbar);
    return 1;
}

int produto_setnome(Produto *p, const char16_t *nome)
{
    if (nome == NULL || nome[0] == u'\0')
        return 0;

    str16_copy(nome, p->nome);
    return 1;
}

int produto_setpreco(Produto *p, float preco)
{
    if (preco <= 0)
        return 0;

    p->preco = preco;
    return 1;
}

int produto_setqt(Produto *p, uint32_t qt)
{
    if (qt < 0)
        return 0;

    p->qt = qt;
    return 1;
}

int produto_setcategoria(Produto *p, const char16_t *categoria)
{
    if (categoria == NULL || categoria[0] == u'\0')
        return 0;

    str16_copy(categoria, p->categoria);
    return 1;
}
