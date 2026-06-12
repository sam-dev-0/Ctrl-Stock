#ifndef ARQUIVO_H
#define ARQUIVO_H

#include "estoque.h"

typedef enum ArquivoStatus
{
    ARQUIVO_OK = 0,
    ARQUIVO_ERRO_ARGUMENTO,
    ARQUIVO_ERRO_MEMORIA,
    ARQUIVO_ERRO_NAO_ENCONTRADO,
    ARQUIVO_ERRO_ABERTURA,
    ARQUIVO_ERRO_FORMATO,
    ARQUIVO_ERRO_DADOS_INVALIDOS,
    ARQUIVO_ERRO_ESTOQUE,
    ARQUIVO_ERRO_ESCRITA,
    ARQUIVO_ERRO_SUBSTITUICAO
} ArquivoStatus;

/*
 * Persiste uma fotografia consistente do estado atual do estoque em dois
 * arquivos tabulares UTF-16LE:
 *
 * - arquivo_produtos: cadastro completo e quantidade atual de cada produto;
 * - arquivo_estoque: linha de controle dos próximos IDs e histórico imutável
 *   das vendas registradas.
 *
 * A gravação ocorre primeiro em arquivos temporários. Os arquivos principais
 * somente são substituídos depois que as duas tabelas temporárias são salvas.
 */
ArquivoStatus arquivo_salvar_estoque(const Estoque *estoque,
                                     const char *arquivo_produtos,
                                     const char *arquivo_estoque);

/*
 * Carrega os dois arquivos em um Estoque temporário. O objeto de destino só é
 * substituído quando todos os registros são válidos. Assim, uma falha de
 * leitura não destrói o estado que já estava em memória.
 *
 * Pré-condição: estoque deve ter sido inicializado com estoque_init().
 */
ArquivoStatus arquivo_carregar_estoque(Estoque *estoque,
                                       const char *arquivo_produtos,
                                       const char *arquivo_estoque);

const char *arquivo_status_texto(ArquivoStatus status);

#endif
