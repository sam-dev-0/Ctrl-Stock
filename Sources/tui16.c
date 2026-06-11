#include "tui16.h"

#ifndef _WIN32
#error "tui16 requer Windows. Crie um backend separado para outros sistemas."
#endif

#include <conio.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define TUI16_ESC "\x1b["
#define TUI16_INPUT_DELAY_MS 15
#define TUI16_TEXT_CHUNK_SIZE 64

/*
 * O backend usa WriteConsoleW para imprimir char16_t diretamente no console.
 * Em Windows, wchar_t e char16_t possuem unidades de código de 16 bits.
 */
_Static_assert(sizeof(wchar_t) == sizeof(char16_t),
               "tui16 requer wchar_t e char16_t com o mesmo tamanho");

static HANDLE tui16_output = INVALID_HANDLE_VALUE;
static DWORD tui16_original_output_mode = 0;
static int tui16_has_original_output_mode = 0;
static int tui16_initialized = 0;
static int tui16_atexit_registered = 0;

/* =========================================================
   FUNÇÕES INTERNAS
========================================================= */

static HANDLE tui16_get_output_handle(void)
{
    if (tui16_output == INVALID_HANDLE_VALUE || tui16_output == NULL)
        tui16_output = GetStdHandle(STD_OUTPUT_HANDLE);

    return tui16_output;
}

static int tui16_write_ascii_n(const char *text, size_t length)
{
    HANDLE output = tui16_get_output_handle();

    if (text == NULL || output == INVALID_HANDLE_VALUE || output == NULL)
        return 0;

    while (length > 0)
    {
        DWORD chunk = length > (size_t)MAXDWORD ? MAXDWORD : (DWORD)length;
        DWORD written = 0;

        if (!WriteConsoleA(output, text, chunk, &written, NULL) || written == 0)
            return 0;

        text += written;
        length -= written;
    }

    return 1;
}

static int tui16_write_ascii(const char *text)
{
    if (text == NULL)
        return 0;

    return tui16_write_ascii_n(text, strlen(text));
}

static int tui16_char16_length(const char16_t *text, size_t *length)
{
    size_t current = 0;

    if (text == NULL || length == NULL)
        return 0;

    while (text[current] != u'\0')
        current++;

    *length = current;
    return 1;
}

static int tui16_char16_length_bounded(
    const char16_t *text,
    size_t capacity,
    size_t *length
)
{
    if (text == NULL || length == NULL || capacity == 0)
        return 0;

    for (size_t current = 0; current < capacity; current++)
    {
        if (text[current] == u'\0')
        {
            *length = current;
            return 1;
        }
    }

    return 0;
}

static int tui16_write_utf16_n(const char16_t *text, size_t length)
{
    HANDLE output = tui16_get_output_handle();

    if (text == NULL || output == INVALID_HANDLE_VALUE || output == NULL)
        return 0;

    while (length > 0)
    {
        DWORD chunk = length > (size_t)MAXDWORD ? MAXDWORD : (DWORD)length;
        DWORD written = 0;

        if (!WriteConsoleW(
                output,
                (const wchar_t *)text,
                chunk,
                &written,
                NULL
            ) || written == 0)
        {
            return 0;
        }

        text += written;
        length -= written;
    }

    return 1;
}

static int tui16_write_utf16(const char16_t *text)
{
    size_t length = 0;

    if (!tui16_char16_length(text, &length))
        return 0;

    return tui16_write_utf16_n(text, length);
}

static void tui16_write_spaces(size_t count)
{
    static const char16_t spaces[TUI16_TEXT_CHUNK_SIZE + 1] =
        u"                                                                ";

    while (count > 0)
    {
        size_t chunk = count > TUI16_TEXT_CHUNK_SIZE
            ? TUI16_TEXT_CHUNK_SIZE
            : count;

        (void)tui16_write_utf16_n(spaces, chunk);
        count -= chunk;
    }
}

static int tui16_is_valid_foreground(int fg)
{
    return fg == 39 ||
           (fg >= 30 && fg <= 37) ||
           (fg >= 90 && fg <= 97);
}

static int tui16_is_valid_background(int bg)
{
    return bg == 49 ||
           (bg >= 40 && bg <= 47) ||
           (bg >= 100 && bg <= 107);
}

static int tui16_is_printable_bmp(wint_t ch)
{
    if (ch < 32 || ch == 127 || ch > UINT16_MAX)
        return 0;

    /* Pares substitutos não são aceitos isoladamente. */
    if (ch >= 0xD800 && ch <= 0xDFFF)
        return 0;

    return 1;
}

static void tui16_finish_input_line(
    int row,
    int col,
    const char16_t *prompt,
    const char16_t *buffer,
    size_t previous_field_width
)
{
    size_t prompt_length = 0;
    size_t buffer_length = 0;

    (void)tui16_char16_length(prompt, &prompt_length);
    (void)tui16_char16_length(buffer, &buffer_length);

    tui16_goto(row, col);
    (void)tui16_write_utf16(prompt);
    (void)tui16_write_utf16(buffer);

    if (previous_field_width > buffer_length)
        tui16_write_spaces(previous_field_width - buffer_length);

    tui16_goto(row, col + (int)prompt_length + (int)buffer_length);
}

/* =========================================================
   INICIALIZAÇÃO E FINALIZAÇÃO
========================================================= */

int tui16_init(void)
{
    HANDLE output;
    DWORD mode = 0;

    if (tui16_initialized)
        return 1;

    output = tui16_get_output_handle();

    if (output == INVALID_HANDLE_VALUE || output == NULL)
        return 0;

    if (!GetConsoleMode(output, &mode))
        return 0;

    tui16_original_output_mode = mode;
    tui16_has_original_output_mode = 1;

    mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (!SetConsoleMode(output, mode))
    {
        tui16_has_original_output_mode = 0;
        return 0;
    }

    tui16_initialized = 1;

    if (!tui16_atexit_registered)
    {
        if (atexit(tui16_shutdown) != 0)
        {
            (void)SetConsoleMode(output, tui16_original_output_mode);
            tui16_has_original_output_mode = 0;
            tui16_initialized = 0;
            return 0;
        }

        tui16_atexit_registered = 1;
    }

    tui16_hidecursor();
    return 1;
}

void tui16_shutdown(void)
{
    HANDLE output = tui16_get_output_handle();

    if (!tui16_initialized)
        return;

    tui16_resetstyle();
    tui16_showcursor();

    if (tui16_has_original_output_mode &&
        output != INVALID_HANDLE_VALUE &&
        output != NULL)
    {
        (void)SetConsoleMode(output, tui16_original_output_mode);
    }

    tui16_has_original_output_mode = 0;
    tui16_initialized = 0;
}

/* =========================================================
   CONSULTA DO TERMINAL
========================================================= */

int tui16_terminal_size(int *width, int *height)
{
    HANDLE output;
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (width == NULL || height == NULL)
        return 0;

    output = tui16_get_output_handle();

    if (output == INVALID_HANDLE_VALUE || output == NULL)
        return 0;

    if (!GetConsoleScreenBufferInfo(output, &info))
        return 0;

    *width = info.srWindow.Right - info.srWindow.Left + 1;
    *height = info.srWindow.Bottom - info.srWindow.Top + 1;

    return 1;
}

/* =========================================================
   OPERAÇÕES BÁSICAS
========================================================= */

void tui16_clear(void)
{
    (void)tui16_write_ascii(TUI16_ESC "2J" TUI16_ESC "H");
}

void tui16_goto(int row, int col)
{
    char sequence[64];
    int length;

    if (row < 1 || col < 1)
        return;

    length = snprintf(sequence, sizeof(sequence), TUI16_ESC "%d;%dH", row, col);

    if (length <= 0 || (size_t)length >= sizeof(sequence))
        return;

    (void)tui16_write_ascii_n(sequence, (size_t)length);
}

void tui16_setcolor(int fg, int bg)
{
    char sequence[64];
    int length;

    if (!tui16_is_valid_foreground(fg) || !tui16_is_valid_background(bg))
        return;

    length = snprintf(sequence, sizeof(sequence), TUI16_ESC "%d;%dm", fg, bg);

    if (length <= 0 || (size_t)length >= sizeof(sequence))
        return;

    (void)tui16_write_ascii_n(sequence, (size_t)length);
}

void tui16_resetstyle(void)
{
    (void)tui16_write_ascii(TUI16_ESC "0m");
}

void tui16_hidecursor(void)
{
    (void)tui16_write_ascii(TUI16_ESC "?25l");
}

void tui16_showcursor(void)
{
    (void)tui16_write_ascii(TUI16_ESC "?25h");
}

/* =========================================================
   COMPONENTES DE DESENHO
========================================================= */

void tui16_drawtext(int row, int col, const char16_t *text)
{
    if (row < 1 || col < 1 || text == NULL)
        return;

    tui16_goto(row, col);
    (void)tui16_write_utf16(text);
}

void tui16_drawbox(int start_row, int start_col, int width, int height)
{
    if (start_row < 1 || start_col < 1 || width < 2 || height < 2)
        return;

    tui16_goto(start_row, start_col);
    (void)tui16_write_utf16(u"┌");

    for (int current = 0; current < width - 2; current++)
        (void)tui16_write_utf16(u"─");

    (void)tui16_write_utf16(u"┐");

    for (int current = 1; current < height - 1; current++)
    {
        tui16_goto(start_row + current, start_col);
        (void)tui16_write_utf16(u"│");

        tui16_goto(start_row + current, start_col + width - 1);
        (void)tui16_write_utf16(u"│");
    }

    tui16_goto(start_row + height - 1, start_col);
    (void)tui16_write_utf16(u"└");

    for (int current = 0; current < width - 2; current++)
        (void)tui16_write_utf16(u"─");

    (void)tui16_write_utf16(u"┘");
}

int tui16_listbox(
    int start_row,
    int start_col,
    const char16_t *const *items,
    int count,
    int selected
)
{
    if (start_row < 1 || start_col < 1 || items == NULL || count <= 0)
        return -1;

    if (selected < 0 || selected >= count)
        return -1;

    size_t maximum_item_length = 0;

    for (int current = 0; current < count; current++)
    {
        size_t item_length = 0;

        if (items[current] == NULL ||
            !tui16_char16_length(items[current], &item_length))
        {
            return -1;
        }

        if (item_length > maximum_item_length)
            maximum_item_length = item_length;
    }

    for (int current = 0; current < count; current++)
    {
        size_t item_length = 0;

        (void)tui16_char16_length(items[current], &item_length);
        tui16_goto(start_row + (current * 2), start_col);

        if (current == selected)
        {
            tui16_setcolor(30, 46);
            (void)tui16_write_utf16(u"> ");
            (void)tui16_write_utf16(items[current]);
            (void)tui16_write_utf16(u" <");
            tui16_write_spaces(maximum_item_length - item_length);
            tui16_resetstyle();
        }
        else
        {
            (void)tui16_write_utf16(u"  ");
            (void)tui16_write_utf16(items[current]);
            (void)tui16_write_utf16(u"  ");
            tui16_write_spaces(maximum_item_length - item_length);
        }
    }

    return selected;
}

/* =========================================================
   ENTRADA DE TEXTO
========================================================= */

tui16_input_result tui16_input(
    int row,
    int col,
    const char16_t *prompt,
    char16_t *buffer,
    size_t buffer_capacity
)
{
    char16_t *original_buffer;
    size_t prompt_length = 0;
    size_t length = 0;
    size_t original_length = 0;
    size_t rendered_field_width = 0;
    int redraw = 1;
    tui16_input_result result = TUI16_INPUT_ERROR;

    if (row < 1 || col < 1 || prompt == NULL || buffer == NULL)
        return TUI16_INPUT_ERROR;

    if (!tui16_char16_length(prompt, &prompt_length))
        return TUI16_INPUT_ERROR;

    if (!tui16_char16_length_bounded(buffer, buffer_capacity, &length))
        return TUI16_INPUT_ERROR;

    original_buffer = malloc((length + 1) * sizeof(*original_buffer));

    if (original_buffer == NULL)
        return TUI16_INPUT_ERROR;

    memcpy(original_buffer, buffer, (length + 1) * sizeof(*buffer));
    original_length = length;

    tui16_showcursor();

    for (;;)
    {
        if (redraw)
        {
            size_t current_field_width = length + 1; /* texto + cursor falso '_' */

            tui16_goto(row, col);
            (void)tui16_write_utf16(prompt);
            (void)tui16_write_utf16(buffer);
            (void)tui16_write_utf16(u"_");

            if (rendered_field_width > current_field_width)
                tui16_write_spaces(rendered_field_width - current_field_width);

            tui16_goto(row, col + (int)prompt_length + (int)length);

            rendered_field_width = current_field_width;
            redraw = 0;
        }

        if (_kbhit())
        {
            wint_t ch = _getwch();

            /* Teclas especiais chegam em duas leituras. */
            if (ch == 0 || ch == 0xE0)
            {
                (void)_getwch();
                continue;
            }

            if (ch == 13)
            {
                result = TUI16_INPUT_CONFIRMED;
                break;
            }

            if (ch == 27)
            {
                memcpy(buffer, original_buffer, (original_length + 1) * sizeof(*buffer));

                /* O comprimento original pode ser diferente do atual. */
                if (!tui16_char16_length_bounded(buffer, buffer_capacity, &length))
                {
                    result = TUI16_INPUT_ERROR;
                    break;
                }

                result = TUI16_INPUT_CANCELLED;
                break;
            }

            if (ch == 8)
            {
                if (length > 0)
                {
                    length--;
                    buffer[length] = u'\0';
                    redraw = 1;
                }

                continue;
            }

            if (tui16_is_printable_bmp(ch) && length + 1 < buffer_capacity)
            {
                buffer[length] = (char16_t)ch;
                length++;
                buffer[length] = u'\0';
                redraw = 1;
            }
        }

        Sleep(TUI16_INPUT_DELAY_MS);
    }

    tui16_finish_input_line(
        row,
        col,
        prompt,
        buffer,
        rendered_field_width
    );

    tui16_hidecursor();
    free(original_buffer);

    return result;
}
