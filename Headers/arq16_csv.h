#ifndef ARQ16_CSV_H
#define ARQ16_CSV_H

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

#include "arq16.h"

typedef struct Header
{
    char *filename_str;
    char16_t *filename;
    char16_t delimiter;

    uint16_t totalcolumns;
    uint16_t *sizecolumn;
    char16_t **labels;
    uint16_t *offsetcolumn;

    size_t totalblanklines;
    size_t capacityblanklines;
    int64_t *blanklines;
} Header;

typedef struct Changes
{
    int64_t row;
    uint16_t column;
    char16_t *old_value;
    char16_t *new_value;
    struct Changes *next;
} Changes;

Header *csv16_create(const char *filename_str, const char16_t *filename_utf16, char16_t delimiter, uint16_t totalcolumns, const uint16_t *sizecolumn, const char16_t *const *labels);

Header *csv16_open(const char *filename_str, const char16_t *filename_utf16, char16_t delimiter);

int csv16_close(Header *header);

/* Lê o estado já persistido. Alterações pendentes aparecem após csv16_save(). */
char16_t *csv16_read(Header *header, const char16_t *target_label, const char16_t *where_label, const char16_t *where_value);

/*
 * Percorre somente registros ativos já persistidos.
 * Os ponteiros em valores são válidos apenas durante a chamada do visitante.
 * Retorne zero no visitante para interromper a iteração com falha.
 */
typedef int (*Csv16Visitante)(int64_t linha, const char16_t *const *valores, uint16_t total_colunas, void *contexto);

int csv16_foreach(Header *header, Csv16Visitante visitante, void *contexto);

Changes *csv16_insert(Header *header, Changes *changes, const char16_t *const *values);
Changes *csv16_update(Header *header, Changes *changes, const char16_t *target_label, const char16_t *new_value, const char16_t *where_label, const char16_t *where_value);
Changes *csv16_delete(Header *header, Changes *changes, const char16_t *where_label, const char16_t *where_value);

/* Retorna NULL no sucesso total ou a parte ainda pendente após falha de escrita. */
Changes *csv16_save(Header *header, Changes *changes);
void csv16_free_changes(Changes *changes);

#endif
