#include <stdlib.h>
#include <string.h>

#include "insert.h"

static void ensure_buffer_initialized(Buffer *buffer) {
  if (buffer->length == 0) {
    buffer->lines = malloc(sizeof(Line));
    if (buffer->lines == NULL) {
      return;
    }
    buffer->lines[0].data = NULL;
    buffer->lines[0].length = 0;
    buffer->lines[0].capacity = 0;
    buffer->length = 1;
  }
}

void insert_char(Window *window, char c) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  ensure_buffer_initialized(buffer);

  Line *line = &buffer->lines[row];

  if (line->length >= line->capacity) {
    size_t new_capacity = line->capacity == 0 ? 16 : line->capacity * 2;
    char *new_data = realloc(line->data, new_capacity);
    if (new_data == NULL) {
      return;
    }
    line->data = new_data;
    line->capacity = new_capacity;
  }

  memmove(line->data + col + 1, line->data + col, line->length - col);
  line->data[col] = c;
  line->length++;
  window->cursor.column++;
}

void insert_newline(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  ensure_buffer_initialized(buffer);

  Line *current_line = &buffer->lines[row];
  size_t old_length = current_line->length;
  char *old_data = current_line->data;

  char *new_current_data = NULL;
  size_t new_current_capacity = 0;
  if (col > 0) {
    new_current_capacity = col < 16 ? 16 : col * 2;
    new_current_data = malloc(new_current_capacity);
    if (new_current_data != NULL) {
      memcpy(new_current_data, old_data, col);
    }
  }

  char *new_line_data = NULL;
  size_t new_line_length = old_length - col;
  size_t new_line_capacity = 0;
  if (new_line_length > 0) {
    new_line_capacity = new_line_length < 16 ? 16 : new_line_length * 2;
    new_line_data = malloc(new_line_capacity);
    if (new_line_data != NULL) {
      memcpy(new_line_data, old_data + col, new_line_length);
    }
  }

  free(old_data);

  buffer->lines = realloc(buffer->lines, sizeof(Line) * (buffer->length + 1));
  memmove(&buffer->lines[row + 2], &buffer->lines[row + 1],
          sizeof(Line) * (buffer->length - row - 1));

  buffer->lines[row].data = new_current_data;
  buffer->lines[row].length = col;
  buffer->lines[row].capacity = new_current_capacity;

  buffer->lines[row + 1].data = new_line_data;
  buffer->lines[row + 1].length = new_line_length;
  buffer->lines[row + 1].capacity = new_line_capacity;

  buffer->length++;

  window->cursor.row++;
  window->cursor.column = 1;
}
