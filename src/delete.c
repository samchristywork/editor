#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "delete.h"

static bool is_word_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
}

static void normalize_selection(size_t *start_row, size_t *start_col,
                                size_t *end_row, size_t *end_col) {
  if (*start_row > *end_row ||
      (*start_row == *end_row && *start_col > *end_col)) {
    size_t tmp = *start_row;
    *start_row = *end_row;
    *end_row = tmp;
    tmp = *start_col;
    *start_col = *end_col;
    *end_col = tmp;
  }
}

void delete_range(Window *window, size_t start_row, size_t start_col,
                  size_t end_row, size_t end_col) {
  Buffer *buffer = window->current_buffer;

  if (buffer->length == 0 || start_row >= buffer->length) {
    return;
  }

  if (end_row >= buffer->length) {
    end_row = buffer->length - 1;
    end_col = buffer->lines[end_row].length;
  }

  if (start_row == end_row) {
    Line *line = &buffer->lines[start_row];

    if (start_col >= line->length || start_col >= end_col) {
      return;
    }

    if (end_col > line->length) {
      end_col = line->length;
    }

    size_t delete_count = end_col - start_col;
    memmove(line->data + start_col, line->data + end_col,
            line->length - end_col);
    line->length -= delete_count;

    if (line->length == 0) {
      free(line->data);
      line->data = NULL;
      line->capacity = 0;
    }
    return;
  }

  Line *start_line = &buffer->lines[start_row];
  Line *end_line = &buffer->lines[end_row];

  if (end_col > end_line->length) {
    end_col = end_line->length;
  }

  size_t new_length = start_col + (end_line->length - end_col);
  char *new_data = NULL;
  size_t new_capacity = 0;

  if (new_length > 0) {
    new_capacity = new_length < 16 ? 16 : new_length * 2;
    new_data = malloc(new_capacity);
    if (new_data != NULL) {
      if (start_col > 0) {
        memcpy(new_data, start_line->data, start_col);
      }
      if (end_col < end_line->length) {
        memcpy(new_data + start_col, end_line->data + end_col,
               end_line->length - end_col);
      }
    }
  }

  for (size_t i = start_row; i <= end_row; i++) {
    free(buffer->lines[i].data);
  }

  buffer->lines[start_row].data = new_data;
  buffer->lines[start_row].length = new_length;
  buffer->lines[start_row].capacity = new_capacity;
  size_t lines_to_remove = end_row - start_row;
  if (lines_to_remove > 0) {
    memmove(&buffer->lines[start_row + 1], &buffer->lines[end_row + 1],
            sizeof(Line) * (buffer->length - end_row - 1));
    buffer->length -= lines_to_remove;
    buffer->lines = realloc(buffer->lines, sizeof(Line) * buffer->length);
  }
}

void delete_char(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  if (buffer->length == 0) {
    return;
  }

  Line *line = &buffer->lines[row];

  if (line->length == 0) {
    return;
  }

  if (col >= line->length) {
    col = line->length - 1;
  }

  delete_range(window, row, col, row, col + 1);
}

void backspace_char(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  if (buffer->length == 0) {
    return;
  }

  Line *line = &buffer->lines[row];

  if (col == 0) {
    if (row == 0) {
      return;
    }

    Line *prev_line = &buffer->lines[row - 1];
    size_t prev_length = prev_line->length;

    if (line->length > 0) {
      size_t new_length = prev_line->length + line->length;
      if (new_length > prev_line->capacity) {
        size_t new_capacity = prev_line->capacity == 0 ? 16 : prev_line->capacity;
        while (new_capacity < new_length) {
          new_capacity *= 2;
        }
        prev_line->data = realloc(prev_line->data, new_capacity);
        prev_line->capacity = new_capacity;
      }
      memcpy(prev_line->data + prev_line->length, line->data, line->length);
      prev_line->length = new_length;
      free(line->data);
    }

    memmove(&buffer->lines[row], &buffer->lines[row + 1],
            sizeof(Line) * (buffer->length - row - 1));

    buffer->length--;
    buffer->lines = realloc(buffer->lines, sizeof(Line) * buffer->length);

    window->cursor.row--;
    window->cursor.column = prev_length + 1;
  } else if (col > 0 && col <= line->length) {
    delete_range(window, row, col - 1, row, col);
    window->cursor.column--;
  }
}

void delete_word(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  if (buffer->length == 0) {
    return;
  }

  Line *line = &buffer->lines[row];

  if (col >= line->length) {
    return;
  }

  if (!is_word_char(line->data[col])) {
    return;
  }

  size_t start = col;
  while (start > 0 && is_word_char(line->data[start - 1])) {
    start--;
  }

  size_t end = col;
  while (end < line->length && is_word_char(line->data[end])) {
    end++;
  }

  delete_range(window, row, start, row, end);
  window->cursor.column = start + 1;
}

void delete_line(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;

  if (buffer->length == 0) {
    return;
  }

  if (buffer->length == 1) {
    Line *line = &buffer->lines[row];
    if (line->data != NULL) {
      free(line->data);
    }
    line->data = NULL;
    line->length = 0;
    line->capacity = 0;
    window->cursor.column = 1;
    return;
  }

  delete_range(window, row, 0, row + 1, 0);

  if (row >= buffer->length) {
    window->cursor.row = buffer->length;
  }
  window->cursor.column = 1;
}

void delete_selection(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Selection *sel = &ctx->selection;

  size_t start_row = sel->start.row;
  size_t start_col = sel->start.column;
  size_t end_row = sel->end.row;
  size_t end_col = sel->end.column;

  normalize_selection(&start_row, &start_col, &end_row, &end_col);

  if (ctx->mode == MODE_LINEWISE_VISUAL) {
    for (size_t i = start_row; i <= end_row; i++) {
      delete_line(window);
      if (window->cursor.row > start_row) {
        window->cursor.row--;
      }
    }
    window->cursor.row = start_row;
    window->cursor.column = 1;
  } else if (ctx->mode == MODE_CHARACTERWISE_VISUAL) {
    delete_range(window, start_row - 1, start_col - 1, end_row - 1, end_col);
    window->cursor.row = start_row;
    window->cursor.column = start_col;
  }
}
