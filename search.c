#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "main.h"
#include "search.h"

bool find_occurrence(Window *window, const char *search_str, size_t search_len,
                     SearchDirection direction) {
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
