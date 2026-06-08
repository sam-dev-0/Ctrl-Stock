#ifndef ARQ16_H
#define ARQ16_H

#include <stdio.h>
#include <stdint.h>
#include <uchar.h>

#include "str16.h"

#define LINE_CONTENT 255u
#define LINE_TOTAL 256u

long arq16_offsetBOM(FILE *file);

int arq16_new(const char *arq16);

int arq16_saveline(FILE *file, const char16_t *txt);

int arq16_save(const char *arq16, char16_t (*txt)[LINE_TOTAL], size_t size);

int arq16_readline(FILE *file, char16_t *line, size_t off, size_t size);

int arq16_read(const char *arq16, char16_t (*out)[LINE_TOTAL], size_t init, size_t size);

int arq16_seek(const char *arq16, const char16_t *key, size_t *index, size_t off, size_t size);

int arq16_alter(const char *arq16, const char16_t *key, const char16_t *var, size_t off, size_t size);

#endif
