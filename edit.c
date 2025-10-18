#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "edit.h"

static void ensure_buffer_initialized(Buffer *buffer) {
  if (buffer->length == 0) {
    buffer->lines = malloc(sizeof(Line));
    if (buffer->lines == NULL) {
      return;
    }
    buffer->lines[0].data = NULL;
    buffer->lines[0].length = 0;
    buffer->length = 1;
  }
}

void insert_char(Window *window, char c) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  ensure_buffer_initialized(buffer);

  Line *line = &buffer->lines[row];

  line->data = realloc(line->data, line->length + 1);
  memmove(line->data + col + 1, line->data + col, line->length - col);
  line->data[col] = c;
  line->length++;
  window->cursor.column++;
}
