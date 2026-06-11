#include "arq16.h"

#include <limits.h>

static int arq16_valid_range(size_t off, size_t size)
{
    if (size < 2 || off > LINE_CONTENT)
        return 0;

    return size - 1 <= LINE_CONTENT - off;
}

static int arq16_row_offset(long bom, size_t row, long *offset)
{
    size_t bytes_per_row = LINE_TOTAL * sizeof(char16_t);

    if (!offset || bom < 0)
        return 0;

    if (row > ((size_t)LONG_MAX - (size_t)bom) / bytes_per_row)
        return 0;

    *offset = bom + (long)(row * bytes_per_row);
    return 1;
}

static int arq16_write_units(FILE *file, const char16_t *txt, size_t size)
{
    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];

    if (!file || !txt || size > LINE_TOTAL)
        return 0;

    for (size_t i = 0; i < size; i++)
    {
        buffer[i * 2] = (unsigned char)(txt[i] & 0xFFu);
        buffer[i * 2 + 1] = (unsigned char)((txt[i] >> 8) & 0xFFu);
    }

    return fwrite(buffer, sizeof(char16_t), size, file) == size;
}

long arq16_offsetBOM(FILE *file)
{
    unsigned char byte[2];
    long pos;
    long offset = 0;

    if (!file)
        return -1;

    pos = ftell(file);
    if (pos < 0 || fseek(file, 0, SEEK_SET) != 0)
        return -1;

    if (fread(byte, 1, sizeof(byte), file) == sizeof(byte) && byte[0] == 0xFFu && byte[1] == 0xFEu)
        offset = 2;

    if (fseek(file, pos, SEEK_SET) != 0)
        return -1;

    return offset;
}

int arq16_new(const char *arq16)
{
    FILE *file;
    int ok;

    if (!arq16)
        return 0;

    file = fopen(arq16, "wb");
    if (!file)
        return 0;

    ok = char16_write(file, 0xFEFFu);
    if (fclose(file) != 0)
        ok = 0;

    return ok;
}

int arq16_saveline(FILE *file, const char16_t *txt)
{
    char16_t line[LINE_TOTAL];

    if (!file || !txt)
        return 0;

    if (!str16_fixsize(txt, line, LINE_TOTAL))
        return 0;

    line[LINE_CONTENT] = u'\n';
    return arq16_write_units(file, line, LINE_TOTAL);
}

int arq16_save(const char *arq16, char16_t (*txt)[LINE_TOTAL], size_t size)
{
    FILE *file;
    int ok = 1;

    if (!arq16 || (!txt && size != 0))
        return 0;

    file = fopen(arq16, "r+b");
    if (!file)
    {
        file = fopen(arq16, "w+b");
        if (!file)
            return 0;

        if (!char16_write(file, 0xFEFFu))
        {
            fclose(file);
            return 0;
        }
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return 0;
    }

    for (size_t i = 0; i < size; i++)
    {
        if (!arq16_saveline(file, txt[i]))
        {
            ok = 0;
            break;
        }
    }

    if (fclose(file) != 0)
        ok = 0;

    return ok;
}

int arq16_readline(FILE *file, char16_t *line, size_t off, size_t size)
{
    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];
    size_t width;

    if (!file || !line || !arq16_valid_range(off, size))
        return 0;

    if (fread(buffer, sizeof(char16_t), LINE_TOTAL, file) != LINE_TOTAL)
        return 0;

    width = size - 1;
    for (size_t i = 0; i < width; i++)
    {
        line[i] = (char16_t)((uint16_t)buffer[(off + i) * 2] |
                            ((uint16_t)buffer[(off + i) * 2 + 1] << 8));
    }

    line[width] = 0;
    return 1;
}

int arq16_read(const char *arq16, char16_t (*out)[LINE_TOTAL], size_t init, size_t size)
{
    FILE *file;
    long bom;
    long offset;

    if (!arq16 || (!out && size != 0))
        return 0;

    file = fopen(arq16, "rb");
    if (!file)
        return 0;

    bom = arq16_offsetBOM(file);
    if (!arq16_row_offset(bom, init, &offset) || fseek(file, offset, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }

    for (size_t i = 0; i < size; i++)
    {
        if (!arq16_readline(file, out[i], 0, LINE_TOTAL))
        {
            fclose(file);
            return 0;
        }
    }

    return fclose(file) == 0;
}

int arq16_seek(const char *arq16, const char16_t *key, size_t *index, size_t off, size_t size)
{
    FILE *file;
    char16_t line[LINE_TOTAL];
    long bom;
    size_t idx = 0;

    if (!arq16 || !key || !index || !arq16_valid_range(off, size))
        return 0;

    file = fopen(arq16, "rb");
    if (!file)
        return 0;

    bom = arq16_offsetBOM(file);
    if (bom < 0 || fseek(file, bom, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }

    while (arq16_readline(file, line, off, size))
    {
        if (str16_contains(line, key))
        {
            *index = idx;
            fclose(file);
            return 1;
        }
        idx++;
    }

    fclose(file);
    return 0;
}

int arq16_alter(const char *arq16, const char16_t *key, const char16_t *var, size_t off, size_t size)
{
    FILE *file;
    char16_t line[LINE_TOTAL];
    char16_t data[LINE_TOTAL];
    long bom;
    long row_offset;
    size_t idx = 0;
    size_t width;
    int found = 0;
    int ok;

    if (!arq16 || !key || !var || !arq16_valid_range(off, size))
        return 0;

    width = size - 1;
    if (str16_length(var) > width)
        return 0;

    file = fopen(arq16, "r+b");
    if (!file)
        return 0;

    bom = arq16_offsetBOM(file);
    if (bom < 0 || fseek(file, bom, SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }

    while (arq16_readline(file, line, off, size))
    {
        if (str16_contains(line, key))
        {
            found = 1;
            break;
        }
        idx++;
    }

    if (!found || !arq16_row_offset(bom, idx, &row_offset))
    {
        fclose(file);
        return 0;
    }

    if (fseek(file, row_offset + (long)(off * sizeof(char16_t)), SEEK_SET) != 0)
    {
        fclose(file);
        return 0;
    }

    if (!str16_pack(var, data, width))
    {
        fclose(file);
        return 0;
    }

    if (off == 0 && size == LINE_TOTAL)
    {
        data[LINE_CONTENT] = u'\n';
        ok = arq16_write_units(file, data, LINE_TOTAL);
    }
    else
    {
        ok = arq16_write_units(file, data, width);
    }

    if (fclose(file) != 0)
        ok = 0;

    return ok;
}
