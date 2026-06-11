#ifndef TUI16_H
#define TUI16_H

#include <stddef.h>
#include <uchar.h>

/*
 * Resultado de tui16_input.
 *
 * TUI16_INPUT_CONFIRMED: o usuário pressionou ENTER.
 * TUI16_INPUT_CANCELLED: o usuário pressionou ESC; o conteúdo anterior é restaurado.
 * TUI16_INPUT_ERROR: argumento inválido, buffer sem terminador ou falha de alocação.
 */
typedef enum
{
    TUI16_INPUT_ERROR = -1,
    TUI16_INPUT_CANCELLED = 0,
    TUI16_INPUT_CONFIRMED = 1
} tui16_input_result;

/* Inicialização e finalização do terminal. */
int tui16_init(void);
void tui16_shutdown(void);

/* Consulta da área visível do terminal. */
int tui16_terminal_size(int *width, int *height);

/* Operações básicas de terminal. */
void tui16_clear(void);
void tui16_goto(int row, int col);
void tui16_setcolor(int fg, int bg);
void tui16_resetstyle(void);
void tui16_hidecursor(void);
void tui16_showcursor(void);

/* Componentes de desenho. */
void tui16_drawbox(int start_row, int start_col, int width, int height);
void tui16_drawtext(int row, int col, const char16_t *text);

/*
 * Desenha uma lista e devolve selected quando os argumentos são válidos.
 * Devolve -1 quando recebe argumentos inválidos.
 */
int tui16_listbox(
    int start_row,
    int start_col,
    const char16_t *const *items,
    int count,
    int selected
);

/*
 * Edita o conteúdo existente de buffer.
 *
 * Regras importantes:
 * - buffer_capacity é a capacidade total do array, incluindo o terminador u'\0';
 * - buffer deve conter uma string terminada dentro dessa capacidade;
 * - para começar com um campo vazio, inicialize buffer[0] = u'\0';
 * - ENTER confirma inclusive uma string vazia;
 * - ESC cancela e restaura o conteúdo que buffer possuía ao entrar na função.
 */
tui16_input_result tui16_input(
    int row,
    int col,
    const char16_t *prompt,
    char16_t *buffer,
    size_t buffer_capacity
);

#endif
