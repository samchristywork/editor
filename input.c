#include <stdlib.h>
#include <unistd.h>

#include "draw.h"
#include "edit.h"
#include "input.h"

#define MILLISECONDS 1000

typedef enum { SEARCH_FORWARD, SEARCH_BACKWARD } SearchDirection;

static bool find_occurrence(Window *window, const char *search_str,
                            size_t search_len, SearchDirection direction) {
  if (search_len == 0) {
    return false;
  }

  Buffer *buffer = window->current_buffer;
  size_t start_row = window->cursor.row - 1;

  if (direction == SEARCH_FORWARD) {
    size_t start_col = window->cursor.column;

    for (size_t row = start_row; row < buffer->length; row++) {
      Line *line = &buffer->lines[row];
      size_t col_start = (row == start_row) ? start_col : 0;

      for (size_t col = col_start; col < line->length; col++) {
        if (col + search_len <= line->length &&
            strncmp(&line->data[col], search_str, search_len) == 0) {
          window->cursor.row = row + 1;
          window->cursor.column = col + 1;
          return true;
        }
      }
    }

    for (size_t row = 0; row < start_row; row++) {
      Line *line = &buffer->lines[row];
      for (size_t col = 0; col < line->length; col++) {
        if (col + search_len <= line->length &&
            strncmp(&line->data[col], search_str, search_len) == 0) {
          window->cursor.row = row + 1;
          window->cursor.column = col + 1;
          return true;
        }
      }
    }
  } else {
    size_t start_col = window->cursor.column - 2;

    if (start_row >= buffer->length) {
      start_row = buffer->length - 1;
    }

    for (size_t row = start_row + 1; row > 0; row--) {
      size_t r = row - 1;
      Line *line = &buffer->lines[r];
      size_t col_end = (r == start_row) ? start_col : line->length;

      if (col_end > line->length) {
        col_end = line->length;
      }

      for (size_t col = col_end + 1; col > 0; col--) {
        size_t c = col - 1;
        if (c + search_len <= line->length &&
            strncmp(&line->data[c], search_str, search_len) == 0) {
          window->cursor.row = r + 1;
          window->cursor.column = c + 1;
          return true;
        }
      }
    }

    for (size_t row = buffer->length; row > start_row + 1; row--) {
      size_t r = row - 1;
      Line *line = &buffer->lines[r];
      for (size_t col = line->length; col > 0; col--) {
        size_t c = col - 1;
        if (c + search_len <= line->length &&
            strncmp(&line->data[c], search_str, search_len) == 0) {
          window->cursor.row = r + 1;
          window->cursor.column = c + 1;
          return true;
        }
      }
    }
  }

  return false;
}

void handle_input(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  bool *running = &ctx->running;
  EditorMode *mode = &ctx->mode;
  char **command_buffer = &ctx->command_buffer;
  size_t *command_buffer_length = &ctx->command_buffer_length;
  char **search_buffer = &ctx->search_buffer;
  size_t *search_buffer_length = &ctx->search_buffer_length;
  size_t width = ctx->terminal.width;
  size_t height = ctx->terminal.height;

  fd_set readfds;
  struct timeval tv;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 100 * MILLISECONDS;

  if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (*mode == MODE_NORMAL) {
        switch (c) {
        case 'h':
          window->cursor.column--;
          break;
        case 'j':
          window->cursor.row++;
          break;
        case 'k':
          window->cursor.row--;
          break;
        case 'l':
          window->cursor.column++;
          break;
        case 'i':
          *mode = MODE_INSERT;
          break;
        case 'q':
          *running = false;
          break;
        }
      } else if (*mode == MODE_INSERT) {
        switch (c) {
        case 27:
          *mode = MODE_NORMAL;
          break;
        case 127:
        case 8:
          delete_char(window);
          break;
        case '\r':
        case '\n':
          insert_newline(window);
          break;
        default:
          if (c >= 32 && c <= 126) {
            insert_char(window, c);
          }
          break;
        }
      }
      draw_screen(window, width, height);
    }
  }
}
