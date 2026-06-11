#ifndef ARQ16_H
#define ARQ16_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <uchar.h>

#include "str16.h"

#define LINE_CONTENT 255u
#define LINE_TOTAL 256u

long arq16_offsetBOM(FILE *file);

int arq16_new(const char *arq16);
int arq16_saveline(FILE *file, const char16_t *txt);
int arq16_save(const char *arq16, char16_t (*txt)[LINE_TOTAL], size_t size);

/*
 * size é a capacidade do buffer line e inclui a posição final reservada a NUL.
 * Assim, size == LINE_TOTAL lê os 255 caracteres úteis de um registro.
 */
int arq16_readline(FILE *file, char16_t *line, size_t off, size_t size);
int arq16_read(const char *arq16, char16_t (*out)[LINE_TOTAL], size_t init, size_t size);

/* Para um campo físico de largura N, passe size == N + 1. */
int arq16_seek(const char *arq16, const char16_t *key, size_t *index, size_t off, size_t size);
int arq16_alter(const char *arq16, const char16_t *key, const char16_t *var, size_t off, size_t size);

#endif
