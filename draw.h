#ifndef DRAW_H
#define DRAW_H

#include "main.h"

void draw_screen(Window *window, size_t width, size_t height, EditorMode mode,
                 Selection *selection, char *command_buffer,
                 size_t command_buffer_length, char *search_buffer,
                 size_t search_buffer_length);

#endif