#ifndef DELETE_H
#define DELETE_H

#include "main.h"

void delete_char(Window *window);

void backspace_char(Window *window);

void delete_line(Window *window);

void delete_word(Window *window);

void delete_selection(Context *ctx);

void delete_range(Window *window, size_t start_row, size_t start_col,
                  size_t end_row, size_t end_col);

#endif
