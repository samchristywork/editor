#ifndef EDIT_H
#define EDIT_H

#include "main.h"

void insert_char(Window *window, char c);

void insert_newline(Window *window);

void delete_char(Window *window);

void backspace_char(Window *window);

void delete_word(Window *window);

void yank_selection(Context *ctx);

void paste_buffer(Context *ctx);

void init_undo_stack(Context *ctx);

void free_undo_stack(Context *ctx);

void push_undo_state(Context *ctx);

void undo(Context *ctx);

#endif
