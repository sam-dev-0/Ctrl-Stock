#include "arq16.h"

long arq16_offsetBOM(FILE* file)
{
    unsigned char byte[2];
    long pos = ftell(file);
    if (pos < 0)
        return 0;

    if (fread(byte, 1, 2, file) != 2)
    {
        fseek(file, pos, SEEK_SET);
        return pos;
    }

    if (byte[0] == 0xFF && byte[1] == 0xFE)
        return 2; /* BOM UTF-16LE */

    fseek(file, pos, SEEK_SET);
    return pos;
}

/* FUNÇÕES PRINCIPAIS */

int arq16_new(const char* arq16)
{
    FILE* file = fopen(arq16, "wb");
    if (!file)
        return 0;

    int ok = char16_write(file, 0xFEFF); /* BOM UTF-16LE */
    fclose(file);
    return ok;
}

int arq16_saveline(FILE* file, const char16_t* txt)
{
    char16_t line[LINE_TOTAL];
    str16_fixsize(txt, line, LINE_TOTAL);
    line[LINE_CONTENT] = u'\n';

    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];
    for (size_t i = 0; i < LINE_TOTAL; i++)
    {
        buffer[i * sizeof(char16_t)] = (unsigned char)(line[i] & 0xFFu);
        buffer[i * sizeof(char16_t) + 1] = (unsigned char)((line[i] >> 8) & 0xFFu);
    }

    if (fwrite(buffer, 1, sizeof(buffer), file) != sizeof(buffer))
        return 0;

    return 1;
}

int arq16_save(const char* arq16, char16_t(* txt)[LINE_TOTAL], size_t size)
{
    FILE* file = fopen(arq16, "r+b");
    if (!file)
    {
        file = fopen(arq16, "w+b");
        if (!file)
            return 0;

        if (!char16_write(file, 0xFEFF))
        {
            fclose(file);
            return 0;
        }
    }

    if (fseek(file, 0, SEEK_END))
    {
        fclose(file);
        return 0;
    }

    for (size_t i = 0; i < size; i++)
    {
        if (!arq16_saveline(file, txt[i]))
        {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

int arq16_readline(FILE* file, char16_t* line, size_t off, size_t size)
{
    if (off > LINE_CONTENT || size + off > LINE_TOTAL || size == 0)
        return 0;

    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];

    if (fread(buffer, 1, LINE_TOTAL * sizeof(char16_t), file) !=
        LINE_TOTAL * sizeof(char16_t))
        return 0;

    for (size_t i = 0; i < size; i++)
        line[i] = (uint16_t)buffer[(off + i) * sizeof(char16_t)] |
                  ((uint16_t)buffer[(off + i) * sizeof(char16_t) + 1] << 8);

    line[size - 1] = 0;
    return 1;
}

int arq16_read(const char* arq16, char16_t(* out)[LINE_TOTAL], size_t init, size_t size)
{
    FILE* file = fopen(arq16, "rb");
    if (!file)
        return 0;

    long BOMoff = arq16_offsetBOM(file);
    if (fseek(file, BOMoff + (long)init * (long)LINE_TOTAL * sizeof(char16_t), SEEK_SET))
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
        out[i][LINE_CONTENT] = 0;
    }

    fclose(file);
    return 1;
}

int arq16_seek(const char* arq16, const char16_t* key, size_t* index, size_t off, size_t size)
{
    if (off > LINE_CONTENT || size + off > LINE_TOTAL || size == 0)
        return 0;

    FILE* file = fopen(arq16, "rb");
    if (!file)
        return 0;

    long BOMoff = arq16_offsetBOM(file);
    if (fseek(file, BOMoff, SEEK_SET))
    {
        fclose(file);
        return 0;
    }

    char16_t line[LINE_TOTAL];
    size_t idx = 0;

    while (arq16_readline(file, line, off, size))
    {
        line[size - 1] = 0;
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

int arq16_alter(const char* arq16, const char16_t* key, const char16_t* var, size_t off, size_t size)
{
    if (off > LINE_CONTENT || size + off > LINE_TOTAL || size == 0)
        return 0;

    if (str16_length(var) > size)
        return 0;

    FILE* file = fopen(arq16, "r+b");
    if (!file)
        return 0;

    long BOMoff = arq16_offsetBOM(file);
    if (fseek(file, BOMoff, SEEK_SET))
    {
        fclose(file);
        return 0;
    }

    char16_t line[LINE_TOTAL];
    size_t idx = 0;
    int found = 0;

    while (arq16_readline(file, line, off, size))
    {
        line[size - 1] = 0;
        if (str16_contains(line, key))
        {
            if (fseek(file, BOMoff + (long)idx * (long)LINE_TOTAL * 
            sizeof(char16_t) + (long)off * sizeof(char16_t), SEEK_SET))
            {
                fclose(file);
                return 0;
            }
            found = 1;
            break;
        }
        idx++;
    }

    if (!found)
    {
        fclose(file);
        return 0;
    }

    str16_fixsize(var, line, size);
    if (size == LINE_TOTAL)
        line[LINE_CONTENT] = u'\n';

    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];
    for (size_t i = 0; i < size; i++)
    {
        buffer[i * sizeof(char16_t)] = (unsigned char)(line[i] & 0xFFu);
        buffer[i * sizeof(char16_t) + 1] = (unsigned char)((line[i] >> 8) & 0xFFu);
    }

    if (fwrite(buffer, 1, size * sizeof(char16_t), file) != size * 
        sizeof(char16_t))
    {
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}
