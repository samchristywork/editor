#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "main.h"

void set_cursor_position(size_t row, size_t column) {
  printf("\x1b[%d;%dH", row, column);
}

void draw_status_bar(size_t width) {
  set_cursor_position(1, 1);
  printf("\x1b[7m");
  char status_bar_text[256];
  snprintf(status_bar_text, 256, "%s %d", "Status Bar", width);
  size_t len = strlen(status_bar_text);
  for (int i = 0; i < width; i++) {
    if (i < len) {
      putchar(status_bar_text[i]);
    } else {
      putchar(' ');
    }
  }
  printf("\x1b[0m");
  fflush(stdout);
}

void draw_line(Window *window, Line line, int n) {
  Cursor cursor = window->cursor;
  set_cursor_position(window->row + n, window->column);
  for (int i = 0; i < window->width; i++) {
    bool on_cursor = cursor.row - 1 == n && cursor.column - 1 == i;
    if (on_cursor) {
      printf("\x1b[7m");
    }
    if (i < line.length) {
      putchar(line.data[i]);
    } else {
      putchar(' ');
    }
    if (on_cursor) {
      printf("\x1b[0m");
    }
  }
}

void constrain_cursor(Window *window) {
  if (window->cursor.row < 1)
    window->cursor.row = 1;
  if (window->cursor.column < 1)
    window->cursor.column = 1;
  if (window->cursor.row > window->current_buffer->length)
    window->cursor.row = window->current_buffer->length;
  if (window->cursor.column >
      window->current_buffer->lines[window->cursor.row - 1].length + 1)
    window->cursor.column =
        window->current_buffer->lines[window->cursor.row - 1].length + 1;
}

void draw_window(Window *window) {
  Buffer *current_buffer = window->current_buffer;
  constrain_cursor(window);

  Line eof_line = {.data = "EOF", .length = 3};
  for (int i = 0; i < window->height; i++) {
    if (i < current_buffer->length) {
      draw_line(window, current_buffer->lines[i], i);
    } else {
      draw_line(window, eof_line, i);
    }
  }
  fflush(stdout);
}

void draw_screen(Window *window, size_t width, size_t height) {
  draw_status_bar(width);
  window->row = 2;
  window->column = 1;
  window->width = width;
  window->height = height;
  draw_window(window);
}
