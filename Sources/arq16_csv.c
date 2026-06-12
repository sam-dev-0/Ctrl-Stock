#include "arq16_csv.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CSV16_INSERT UINT16_C(0xFFFE)
#define CSV16_DELETE UINT16_C(0xFFFF)
#define CSV16_BLANK_INIT 10u
#define CSV16_FORMAT_VERSION 1u

static char16_t *csv16_dup16(const char16_t *txt)
{
    char16_t *copy;
    size_t len;

    if (!txt)
        return NULL;

    len = str16_length(txt);
    copy = malloc((len + 1) * sizeof(*copy));
    if (!copy)
        return NULL;

    return str16_copy(txt, copy);
}

static int csv16_mul_size(size_t a, size_t b, size_t *resultado)
{
    if (resultado == NULL || (a != 0U && b > SIZE_MAX / a))
        return 0;

    *resultado = a * b;
    return 1;
}

static char *csv16_dup8(const char *txt)
{
    char *copy;
    size_t len;

    if (!txt)
        return NULL;

    len = strlen(txt);
    copy = malloc(len + 1);
    if (!copy)
        return NULL;

    memcpy(copy, txt, len + 1);
    return copy;
}

static int csv16_delim_valid(char16_t delim)
{
    return delim != 0 && delim != u'\n' && delim != u'\r' &&
           delim != u' ' && delim != (char16_t)CSV16_DELETE;
}

static int csv16_label_valid(const char16_t *label)
{
    if (!label || !*label)
        return 0;

    return !str16_char_contains(u'|', label) && !str16_char_contains(u':', label);
}

static int csv16_value_valid(const char16_t *value)
{
    if (!value)
        return 0;

    while (*value)
    {
        if (*value == u'\n' || *value == u'\r' ||
            *value == (char16_t)CSV16_DELETE)
            return 0;
        value++;
    }

    return 1;
}

static int csv16_schema_valid(const char16_t *filename, char16_t delim, uint16_t columns, const uint16_t *sizes, const char16_t *const *labels)
{
    size_t row_size = 0;

    if (!filename || !*filename || !sizes || !labels || columns == 0 ||
        !csv16_delim_valid(delim) || str16_char_contains(u'|', filename))
        return 0;

    for (uint16_t i = 0; i < columns; i++)
    {
        if (sizes[i] == 0 || !csv16_label_valid(labels[i]))
            return 0;

        row_size += sizes[i];
        if (i > 0)
            row_size++;

        if (row_size > LINE_CONTENT)
            return 0;

        for (uint16_t j = 0; j < i; j++)
            if (str16_equal(labels[i], labels[j]))
                return 0;
    }

    return 1;
}

static Header *csv16_new_header(const char *filename_str, const char16_t *filename_utf16, char16_t delimiter, uint16_t totalcolumns, const uint16_t *sizecolumn, const char16_t *const *labels)
{
    Header *header;

    if (!filename_str || !csv16_schema_valid(filename_utf16, delimiter, totalcolumns, sizecolumn, labels))
        return NULL;

    header = calloc(1, sizeof(*header));
    if (!header)
        return NULL;

    header->filename_str = csv16_dup8(filename_str);
    header->filename = csv16_dup16(filename_utf16);
    header->delimiter = delimiter;
    header->totalcolumns = totalcolumns;
    header->sizecolumn = malloc(totalcolumns * sizeof(*header->sizecolumn));
    header->labels = calloc(totalcolumns, sizeof(*header->labels));
    header->offsetcolumn = malloc(totalcolumns * sizeof(*header->offsetcolumn));
    header->blanklines = malloc(CSV16_BLANK_INIT * sizeof(*header->blanklines));

    if (!header->filename_str || !header->filename || !header->sizecolumn ||
        !header->labels || !header->offsetcolumn || !header->blanklines)
    {
        csv16_close(header);
        return NULL;
    }

    header->capacityblanklines = CSV16_BLANK_INIT;
    header->offsetcolumn[0] = 0;

    for (uint16_t i = 0; i < totalcolumns; i++)
    {
        header->sizecolumn[i] = sizecolumn[i];
        header->labels[i] = csv16_dup16(labels[i]);
        if (!header->labels[i])
        {
            csv16_close(header);
            return NULL;
        }

        if (i > 0)
        {
            header->offsetcolumn[i] = (uint16_t)(header->offsetcolumn[i - 1] + header->sizecolumn[i - 1] + 1u);
        }
    }

    return header;
}

int csv16_close(Header *header)
{
    if (!header)
        return 1;

    if (header->labels)
    {
        for (uint16_t i = 0; i < header->totalcolumns; i++)
            free(header->labels[i]);
    }

    free(header->filename_str);
    free(header->filename);
    free(header->sizecolumn);
    free(header->labels);
    free(header->offsetcolumn);
    free(header->blanklines);
    free(header);
    return 1;
}

static int csv16_meta_line(const Header *header, char16_t line[LINE_TOTAL])
{
    char16_t num[32];

    if (!header || !line)
        return 0;

    line[0] = 0;
    if (!str16_concat_n(header->filename, line, LINE_TOTAL) ||
        !str16_concat_n(u"|", line, LINE_TOTAL) ||
        !str16_fromLong_n(CSV16_FORMAT_VERSION, num, 32, 10) ||
        !str16_concat_n(num, line, LINE_TOTAL) ||
        !str16_concat_n(u"|", line, LINE_TOTAL) ||
        !str16_fromLong_n((long)header->delimiter, num, 32, 10) ||
        !str16_concat_n(num, line, LINE_TOTAL) ||
        !str16_concat_n(u"|", line, LINE_TOTAL) ||
        !str16_fromLong_n(header->totalcolumns, num, 32, 10) ||
        !str16_concat_n(num, line, LINE_TOTAL) ||
        !str16_concat_n(u"|", line, LINE_TOTAL))
        return 0;

    for (uint16_t i = 0; i < header->totalcolumns; i++)
    {
        if (!str16_concat_n(header->labels[i], line, LINE_TOTAL) ||
            !str16_concat_n(u":", line, LINE_TOTAL) ||
            !str16_fromLong_n(header->sizecolumn[i], num, 32, 10) ||
            !str16_concat_n(num, line, LINE_TOTAL))
            return 0;

        if (i + 1 < header->totalcolumns && !str16_concat_n(u"|", line, LINE_TOTAL))
            return 0;
    }

    return str16_length(line) <= LINE_CONTENT;
}

Header *csv16_create(const char *filename_str, const char16_t *filename_utf16, char16_t delimiter, uint16_t totalcolumns, const uint16_t *sizecolumn, const char16_t *const *labels)
{
    Header *header;
    char16_t line[LINE_TOTAL];

    header = csv16_new_header(filename_str, filename_utf16, delimiter, totalcolumns, sizecolumn, labels);
    if (!header)
        return NULL;

    if (!csv16_meta_line(header, line))
    {
        csv16_close(header);
        return NULL;
    }

    if (!arq16_new(filename_str))
    {
        csv16_close(header);
        return NULL;
    }

    if (!arq16_save(filename_str, &line, 1))
    {
        remove(filename_str);
        csv16_close(header);
        return NULL;
    }

    return header;
}

static char16_t *csv16_token(char16_t *string, char16_t delim, char16_t **save)
{
    char16_t *start;
    char16_t *end;

    if (!save)
        return NULL;

    if (string)
        *save = string;

    if (!*save)
        return NULL;

    start = *save;
    end = start;

    while (*end && *end != delim)
        end++;

    if (*end == delim)
    {
        *end = 0;
        *save = end + 1;
    }
    else
    {
        *save = NULL;
    }

    return start;
}

static size_t csv16_split(char16_t *line, char16_t delim, char16_t **out, size_t max_tokens)
{
    char16_t *save = NULL;
    char16_t *token;
    size_t count = 0;

    if (!line || !out || max_tokens == 0)
        return 0;

    token = csv16_token(line, delim, &save);
    while (token && count < max_tokens)
    {
        out[count++] = token;
        token = csv16_token(NULL, delim, &save);
    }

    return token ? max_tokens + 1 : count;
}

static int csv16_parse_u16(const char16_t *txt, uint16_t *value)
{
    char16_t *end;
    long parsed;

    if (!txt || !*txt || !value)
        return 0;

    parsed = str16_forLong(txt, &end, 10);
    if (end == txt || *end != 0 || parsed <= 0 || parsed > UINT16_MAX)
        return 0;

    *value = (uint16_t)parsed;
    return 1;
}

static int csv16_reserve_blank(Header *header)
{
    int64_t *tmp;
    size_t capacity;

    if (header->totalblanklines < header->capacityblanklines)
        return 1;

    if (header->capacityblanklines > SIZE_MAX / 2)
        return 0;

    capacity = header->capacityblanklines * 2;
    tmp = realloc(header->blanklines, capacity * sizeof(*tmp));
    if (!tmp)
        return 0;

    header->blanklines = tmp;
    header->capacityblanklines = capacity;
    return 1;
}

static int csv16_add_blank(Header *header, int64_t row)
{
    for (size_t i = 0; i < header->totalblanklines; i++)
        if (header->blanklines[i] == row)
            return 1;

    if (!csv16_reserve_blank(header))
        return 0;

    header->blanklines[header->totalblanklines++] = row;
    return 1;
}

static int csv16_row_offset(long bom, int64_t row, long *offset)
{
    size_t bytes_per_row = LINE_TOTAL * sizeof(char16_t);

    if (!offset || bom < 0 || row < 0)
        return 0;

    if ((uint64_t)row > ((uint64_t)LONG_MAX - (uint64_t)bom) / bytes_per_row)
        return 0;

    *offset = bom + (long)((uint64_t)row * bytes_per_row);
    return 1;
}

static int csv16_file_valid(FILE *file, long bom)
{
    long size;
    long data_size;

    /* Arquivos gerenciados por csv16 devem ser UTF-16LE com BOM. */
    if (!file || bom != 2L || fseek(file, 0, SEEK_END) != 0)
        return 0;

    size = ftell(file);
    if (size < bom)
        return 0;

    data_size = size - bom;
    return data_size >= (long)(LINE_TOTAL * sizeof(char16_t)) &&
           data_size % (long)(LINE_TOTAL * sizeof(char16_t)) == 0;
}

static int csv16_row_valid(const Header *header, const char16_t row[LINE_TOTAL])
{
    if (!header || !row)
        return 0;

    if (row[0] == (char16_t)CSV16_DELETE)
        return 1;

    for (uint16_t i = 0; i + 1 < header->totalcolumns; i++)
    {
        size_t pos = (size_t)header->offsetcolumn[i] + header->sizecolumn[i];
        if (row[pos] != header->delimiter)
            return 0;
    }

    return 1;
}

Header *csv16_open(const char *filename_str, const char16_t *filename_utf16, char16_t delimiter)
{
    FILE *file;
    Header *header = NULL;
    char16_t header_line[LINE_TOTAL];
    char16_t work[LINE_TOTAL];
    char16_t *tokens[LINE_TOTAL];
    const char16_t **labels = NULL;
    uint16_t *sizes = NULL;
    uint16_t version;
    uint16_t delimiter_lido;
    uint16_t columns;
    size_t token_count;
    long bom;

    if (!filename_str || !filename_utf16 || !csv16_delim_valid(delimiter))
        return NULL;

    file = fopen(filename_str, "rb");
    if (!file)
        return NULL;

    bom = arq16_offsetBOM(file);
    if (!csv16_file_valid(file, bom) || fseek(file, bom, SEEK_SET) != 0 ||
        !arq16_readline(file, header_line, 0, LINE_TOTAL))
        goto fail;

    str16_trimspaces_right(header_line);
    if (!str16_copy_n(header_line, work, LINE_TOTAL))
        goto fail;

    token_count = csv16_split(work, u'|', tokens, LINE_TOTAL);
    if (token_count < 5 || !str16_equal(tokens[0], filename_utf16) ||
        !csv16_parse_u16(tokens[1], &version) ||
        version != CSV16_FORMAT_VERSION ||
        !csv16_parse_u16(tokens[2], &delimiter_lido) ||
        delimiter_lido != (uint16_t)delimiter ||
        !csv16_parse_u16(tokens[3], &columns) ||
        token_count != (size_t)columns + 4U)
        goto fail;

    labels = calloc(columns, sizeof(*labels));
    sizes = malloc(columns * sizeof(*sizes));
    if (!labels || !sizes)
        goto fail;

    for (uint16_t i = 0; i < columns; i++)
    {
        char16_t *part[3];
        size_t count = csv16_split(tokens[i + 4], u':', part, 3);

        if (count != 2 || !csv16_parse_u16(part[1], &sizes[i]))
            goto fail;

        labels[i] = part[0];
    }

    header = csv16_new_header(filename_str, filename_utf16, delimiter, columns, sizes, labels);
    if (!header)
        goto fail;

    if (fseek(file, bom + (long)(LINE_TOTAL * sizeof(char16_t)), SEEK_SET) != 0)
        goto fail;

    for (int64_t row = 1;; row++)
    {
        char16_t row_line[LINE_TOTAL];

        if (!arq16_readline(file, row_line, 0, LINE_TOTAL))
            break;

        if (!csv16_row_valid(header, row_line))
            goto fail;

        if (row_line[0] == (char16_t)CSV16_DELETE && !csv16_add_blank(header, row))
            goto fail;
    }

    free(labels);
    free(sizes);
    fclose(file);
    return header;

fail:
    free(labels);
    free(sizes);
    csv16_close(header);
    fclose(file);
    return NULL;
}

static int csv16_get_col(const Header *header, const char16_t *label)
{
    if (!header || !label)
        return -1;

    for (uint16_t i = 0; i < header->totalcolumns; i++)
        if (str16_equal(header->labels[i], label))
            return (int)i;

    return -1;
}

static int csv16_get_field(const Header *header, const char16_t *row, uint16_t column, char16_t out[LINE_TOTAL])
{
    uint16_t width;
    uint16_t offset;

    if (!header || !row || !out || column >= header->totalcolumns)
        return 0;

    width = header->sizecolumn[column];
    offset = header->offsetcolumn[column];

    for (uint16_t i = 0; i < width; i++)
        out[i] = row[offset + i];

    out[width] = 0;
    str16_trimspaces_right(out);
    return 1;
}

static int csv16_set_field(const Header *header, char16_t *row, uint16_t column, const char16_t *value)
{
    if (!header || !row || !value || column >= header->totalcolumns)
        return 0;

    if (!csv16_value_valid(value))
        return 0;

    return str16_pack(value, row + header->offsetcolumn[column], header->sizecolumn[column]);
}

static int csv16_make_row(const Header *header, const char16_t *const *values, char16_t row[LINE_TOTAL])
{
    if (!header || !values || !row)
        return 0;

    for (size_t i = 0; i < LINE_CONTENT; i++)
        row[i] = u' ';
    row[LINE_CONTENT] = u'\n';

    for (uint16_t i = 0; i < header->totalcolumns; i++)
    {
        if (!values[i] || !csv16_set_field(header, row, i, values[i]))
            return 0;

        if (i + 1 < header->totalcolumns)
            row[header->offsetcolumn[i] + header->sizecolumn[i]] = header->delimiter;
    }

    return row[0] != (char16_t)CSV16_DELETE;
}

static Changes *csv16_append(Changes *head, Changes *node)
{
    Changes *curr;

    if (!node)
        return head;
    if (!head)
        return node;

    curr = head;
    while (curr->next)
        curr = curr->next;

    curr->next = node;
    return head;
}

void csv16_free_changes(Changes *changes)
{
    while (changes)
    {
        Changes *next = changes->next;
        free(changes->old_value);
        free(changes->new_value);
        free(changes);
        changes = next;
    }
}

static int csv16_apply_changes(const Header *header, const Changes *changes, int64_t row, char16_t row_line[LINE_TOTAL])
{
    for (const Changes *curr = changes; curr; curr = curr->next)
    {
        if (curr->row != row)
            continue;

        if (curr->column == CSV16_DELETE)
        {
            row_line[0] = (char16_t)CSV16_DELETE;
        }
        else if (curr->column != CSV16_INSERT)
        {
            if (!csv16_set_field(header, row_line, curr->column, curr->new_value))
                return 0;
        }
    }

    return 1;
}

static int64_t csv16_find_disk(const Header *header, const Changes *changes, uint16_t where_col, const char16_t *where_value)
{
    FILE *file;
    char16_t row_line[LINE_TOTAL];
    char16_t field[LINE_TOTAL];
    long bom;
    int64_t row = 1;

    file = fopen(header->filename_str, "rb");
    if (!file)
        return -1;

    bom = arq16_offsetBOM(file);
    if (bom < 0 || fseek(file, bom + (long)(LINE_TOTAL * sizeof(char16_t)), SEEK_SET) != 0)
    {
        fclose(file);
        return -1;
    }

    while (arq16_readline(file, row_line, 0, LINE_TOTAL))
    {
        if (row_line[0] != (char16_t)CSV16_DELETE &&
            csv16_apply_changes(header, changes, row, row_line) &&
            row_line[0] != (char16_t)CSV16_DELETE &&
            csv16_get_field(header, row_line, where_col, field) &&
            str16_equal(field, where_value))
        {
            fclose(file);
            return row;
        }
        row++;
    }

    fclose(file);
    return -1;
}

static Changes *csv16_find_insert(const Header *header, Changes *changes, uint16_t where_col, const char16_t *where_value)
{
    char16_t field[LINE_TOTAL];

    for (Changes *curr = changes; curr; curr = curr->next)
    {
        if (curr->column == CSV16_INSERT &&
            csv16_get_field(header, curr->new_value, where_col, field) &&
            str16_equal(field, where_value))
            return curr;
    }

    return NULL;
}

static Changes *csv16_find_update(Changes *changes, int64_t row, uint16_t column)
{
    Changes *found = NULL;

    for (Changes *curr = changes; curr; curr = curr->next)
        if (curr->row == row && curr->column == column)
            found = curr;

    return found;
}

static Changes *csv16_remove_node(Changes *head, Changes *target)
{
    Changes *prev = NULL;
    Changes *curr = head;

    while (curr)
    {
        if (curr == target)
        {
            if (prev)
                prev->next = curr->next;
            else
                head = curr->next;

            curr->next = NULL;
            csv16_free_changes(curr);
            return head;
        }
        prev = curr;
        curr = curr->next;
    }

    return head;
}

char16_t *csv16_read(Header *header, const char16_t *target_label, const char16_t *where_label, const char16_t *where_value)
{
    FILE *file;
    char16_t row_line[LINE_TOTAL];
    char16_t field[LINE_TOTAL];
    char16_t *result;
    int target_col;
    int where_col;
    int64_t row;
    long bom;
    long offset;

    if (!header || !where_value)
        return NULL;

    target_col = csv16_get_col(header, target_label);
    where_col = csv16_get_col(header, where_label);
    if (target_col < 0 || where_col < 0)
        return NULL;

    row = csv16_find_disk(header, NULL, (uint16_t)where_col, where_value);
    if (row < 0)
        return NULL;

    file = fopen(header->filename_str, "rb");
    if (!file)
        return NULL;

    bom = arq16_offsetBOM(file);
    if (!csv16_row_offset(bom, row, &offset) || fseek(file, offset, SEEK_SET) != 0 ||
        !arq16_readline(file, row_line, 0, LINE_TOTAL) ||
        !csv16_get_field(header, row_line, (uint16_t)target_col, field))
    {
        fclose(file);
        return NULL;
    }

    fclose(file);
    result = csv16_dup16(field);
    return result;
}

int csv16_foreach(Header *header, Csv16Visitante visitante, void *contexto)
{
    FILE *file;
    char16_t row_line[LINE_TOTAL];
    char16_t **valores = NULL;
    char16_t *armazenamento = NULL;
    long bom;
    int64_t linha = 1;
    int ok = 1;

    if (!header || !visitante || header->totalcolumns == 0U)
        return 0;

    {
        size_t bytes_valores;
        size_t total_unidades;
        size_t bytes_armazenamento;

        if (!csv16_mul_size((size_t)header->totalcolumns, sizeof(*valores), &bytes_valores) || !csv16_mul_size((size_t)header->totalcolumns, LINE_TOTAL, &total_unidades) || !csv16_mul_size(total_unidades, sizeof(*armazenamento), &bytes_armazenamento))
        {
            return 0;
        }

        valores = malloc(bytes_valores);
        armazenamento = malloc(bytes_armazenamento);
    }
    if (!valores || !armazenamento)
    {
        free(valores);
        free(armazenamento);
        return 0;
    }

    for (uint16_t coluna = 0; coluna < header->totalcolumns; ++coluna)
        valores[coluna] = armazenamento + (size_t)coluna * LINE_TOTAL;

    file = fopen(header->filename_str, "rb");
    if (!file)
    {
        free(valores);
        free(armazenamento);
        return 0;
    }

    bom = arq16_offsetBOM(file);
    if (!csv16_file_valid(file, bom) ||
        fseek(file, bom + (long)(LINE_TOTAL * sizeof(char16_t)), SEEK_SET) != 0)
    {
        fclose(file);
        free(valores);
        free(armazenamento);
        return 0;
    }

    while (arq16_readline(file, row_line, 0, LINE_TOTAL))
    {
        if (!csv16_row_valid(header, row_line))
        {
            ok = 0;
            break;
        }

        if (row_line[0] != (char16_t)CSV16_DELETE)
        {
            for (uint16_t coluna = 0; coluna < header->totalcolumns; ++coluna)
            {
                if (!csv16_get_field(header, row_line, coluna, valores[coluna]))
                {
                    ok = 0;
                    break;
                }
            }

            if (!ok || !visitante(linha, (const char16_t *const *)valores, header->totalcolumns, contexto))
            {
                ok = 0;
                break;
            }
        }

        ++linha;
    }

    if (ferror(file))
        ok = 0;

    if (fclose(file) != 0)
        ok = 0;

    free(valores);
    free(armazenamento);
    return ok;
}

Changes *csv16_insert(Header *header, Changes *changes, const char16_t *const *values)
{
    Changes *node;

    if (!header || !values)
        return changes;

    node = calloc(1, sizeof(*node));
    if (!node)
        return changes;

    node->new_value = malloc(LINE_TOTAL * sizeof(*node->new_value));
    if (!node->new_value || !csv16_make_row(header, values, node->new_value))
    {
        csv16_free_changes(node);
        return changes;
    }

    node->row = -1;
    node->column = CSV16_INSERT;
    return csv16_append(changes, node);
}

Changes *csv16_update(Header *header, Changes *changes, const char16_t *target_label, const char16_t *new_value, const char16_t *where_label, const char16_t *where_value)
{
    Changes *node;
    Changes *insert;
    Changes *update;
    char16_t *copy;
    int target_col;
    int where_col;
    int64_t row;

    if (!header || !new_value || !where_value)
        return changes;

    target_col = csv16_get_col(header, target_label);
    where_col = csv16_get_col(header, where_label);
    if (target_col < 0 || where_col < 0 || !csv16_value_valid(new_value) ||
        str16_length(new_value) > header->sizecolumn[target_col])
        return changes;

    insert = csv16_find_insert(header, changes, (uint16_t)where_col, where_value);
    if (insert)
    {
        csv16_set_field(header, insert->new_value, (uint16_t)target_col, new_value);
        return changes;
    }

    row = csv16_find_disk(header, changes, (uint16_t)where_col, where_value);
    if (row < 0)
        return changes;

    update = csv16_find_update(changes, row, (uint16_t)target_col);
    copy = csv16_dup16(new_value);
    if (!copy)
        return changes;

    if (update)
    {
        free(update->new_value);
        update->new_value = copy;
        return changes;
    }

    node = calloc(1, sizeof(*node));
    if (!node)
    {
        free(copy);
        return changes;
    }

    node->row = row;
    node->column = (uint16_t)target_col;
    node->new_value = copy;
    return csv16_append(changes, node);
}

Changes *csv16_delete(Header *header, Changes *changes, const char16_t *where_label, const char16_t *where_value)
{
    Changes *insert;
    Changes *node;
    int where_col;
    int64_t row;

    if (!header || !where_value)
        return changes;

    where_col = csv16_get_col(header, where_label);
    if (where_col < 0)
        return changes;

    insert = csv16_find_insert(header, changes, (uint16_t)where_col, where_value);
    if (insert)
        return csv16_remove_node(changes, insert);

    row = csv16_find_disk(header, changes, (uint16_t)where_col, where_value);
    if (row < 0)
        return changes;

    node = calloc(1, sizeof(*node));
    if (!node)
        return changes;

    node->row = row;
    node->column = CSV16_DELETE;
    return csv16_append(changes, node);
}

static int csv16_write_at(FILE *file, long offset, const char16_t *data, size_t len)
{
    unsigned char buffer[LINE_TOTAL * sizeof(char16_t)];

    if (!file || !data || len > LINE_TOTAL || fseek(file, offset, SEEK_SET) != 0)
        return 0;

    for (size_t i = 0; i < len; i++)
    {
        buffer[i * 2] = (unsigned char)(data[i] & 0xFFu);
        buffer[i * 2 + 1] = (unsigned char)((data[i] >> 8) & 0xFFu);
    }

    return fwrite(buffer, sizeof(char16_t), len, file) == len && fflush(file) == 0;
}

static int csv16_save_insert(Header *header, FILE *file, long bom, Changes *change)
{
    int64_t row;
    long offset;

    if (header->totalblanklines > 0)
    {
        row = header->blanklines[header->totalblanklines - 1];
    }
    else
    {
        long file_size;

        if (fseek(file, 0, SEEK_END) != 0)
            return 0;

        file_size = ftell(file);
        if (file_size < bom)
            return 0;

        row = (file_size - bom) / (long)(LINE_TOTAL * sizeof(char16_t));
    }

    if (!csv16_row_offset(bom, row, &offset) ||
        !csv16_write_at(file, offset, change->new_value, LINE_TOTAL))
        return 0;

    change->row = row;
    if (header->totalblanklines > 0)
        header->totalblanklines--;

    return 1;
}

static int csv16_save_delete(Header *header, FILE *file, long offset, int64_t row)
{
    char16_t blank[LINE_TOTAL];

    if (!csv16_reserve_blank(header))
        return 0;

    blank[0] = (char16_t)CSV16_DELETE;
    for (size_t i = 1; i < LINE_CONTENT; i++)
        blank[i] = u' ';
    blank[LINE_CONTENT] = u'\n';

    return csv16_write_at(file, offset, blank, LINE_TOTAL) && csv16_add_blank(header, row);
}

static int csv16_save_update(const Header *header, FILE *file, long offset, const Changes *change)
{
    char16_t field[LINE_CONTENT];
    uint16_t width;

    if (change->column >= header->totalcolumns ||
        !csv16_value_valid(change->new_value))
        return 0;

    width = header->sizecolumn[change->column];
    if (!str16_pack(change->new_value, field, width))
        return 0;

    offset += (long)(header->offsetcolumn[change->column] * sizeof(char16_t));
    return csv16_write_at(file, offset, field, width);
}

Changes *csv16_save(Header *header, Changes *changes)
{
    FILE *file;
    Changes *curr;
    long bom;

    if (!changes)
        return NULL;
    if (!header)
        return changes;

    file = fopen(header->filename_str, "r+b");
    if (!file)
        return changes;

    bom = arq16_offsetBOM(file);
    if (!csv16_file_valid(file, bom))
    {
        fclose(file);
        return changes;
    }

    curr = changes;
    while (curr)
    {
        Changes *next = curr->next;
        long offset = 0;
        int ok;

        if (curr->column == CSV16_INSERT)
        {
            ok = csv16_save_insert(header, file, bom, curr);
        }
        else if (!csv16_row_offset(bom, curr->row, &offset))
        {
            ok = 0;
        }
        else if (curr->column == CSV16_DELETE)
        {
            ok = csv16_save_delete(header, file, offset, curr->row);
        }
        else
        {
            ok = csv16_save_update(header, file, offset, curr);
        }

        if (!ok)
        {
            fclose(file);
            return curr;
        }

        curr->next = NULL;
        csv16_free_changes(curr);
        curr = next;
    }

    fclose(file);
    return NULL;
}
