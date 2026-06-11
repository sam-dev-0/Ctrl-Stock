#ifndef STR16_H
#define STR16_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <uchar.h>

size_t str16_length(const char16_t *string);

char16_t *str16_copy(const char16_t *in, char16_t *out);
int str16_copy_n(const char16_t *in, char16_t *out, size_t capacity);

char16_t *str16_concat(const char16_t *in, char16_t *out);
int str16_concat_n(const char16_t *in, char16_t *out, size_t capacity);

int str16_equal(const char16_t *str1, const char16_t *str2);
int str16_contains(const char16_t *string, const char16_t *sub);

/*
 * Copia uma string para um buffer de capacidade fixsize.
 * A última posição do buffer é reservada para o terminador NUL.
 */
int str16_fixsize(const char16_t *in, char16_t *out, size_t fixsize);

/*
 * Empacota exatamente width unidades UTF-16 para armazenamento fixo.
 * O resultado não recebe terminador NUL; posições restantes viram espaços.
 */
int str16_pack(const char16_t *in, char16_t *out, size_t width);

void str16_trimspaces_right(char16_t *string);
void str16_trimspaces_left(char16_t *string);

char16_t *str16_reverse(char16_t *string);

void str16_split(const char16_t *string, char16_t **out, char16_t delim);
int str16_char_contains(char16_t c, const char16_t *string);
char16_t *str16_tokens(char16_t *string, const char16_t *delim, char16_t **save);

int char16_write(FILE *stream, uint16_t c);
int char16_read(FILE *file, uint16_t *c);

int str16_print(const char16_t *txt);
int str16_println(const char16_t *txt);
int str16_scanline(char16_t *buffer, size_t max_len);

/* Escopo intencional: caracteres do BMP; surrogates são substituídos por '?'. */
char *str16_to_utf8_alloc(const char16_t *txt);

long int str16_forLong(const char16_t *string, char16_t **end, int base);
double str16_forDouble(const char16_t *string, char16_t **end);

/* O chamador deve fornecer espaço suficiente para a representação. */
char16_t *str16_fromLong(long value, char16_t *buffer, int base);
char16_t *str16_fromLong_n(long value, char16_t *buffer, size_t max_len, int base);
char16_t *str16_fromDouble(double value, int precision, char16_t *buffer, size_t max_len);

#endif
