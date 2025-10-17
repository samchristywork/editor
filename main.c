#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "main.h"

#define MILLISECONDS 1000
Context *global_ctx;

void enter_alt_screen() {
  struct termios raw = global_ctx->terminal.attrs;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  printf("\x1b[?1049h");
  printf("\x1b[?25l");
  printf("\x1b[2J");
  printf("\x1b[H");
  fflush(stdout);
}

void leave_alt_screen(Context ctx) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx.terminal.attrs);
  printf("\x1b[?1049l");
  printf("\x1b[?25h");
  fflush(stdout);
}

Buffer *create_buffer_from_file(File file) {
  FILE *f = fopen(file.name, "r");
  if (f == NULL) {
    return NULL;
  }

  Buffer *buffer = malloc(sizeof(Buffer));
  if (buffer == NULL) {
    fclose(f);
    return NULL;
  }
  buffer->file = file;
  buffer->lines = NULL;
  buffer->length = 0;

  char *line_buf = NULL;
  size_t line_buf_size = 0;
  ssize_t line_length;

  while ((line_length = getline(&line_buf, &line_buf_size, f)) != -1) {
    if (line_length > 0 && line_buf[line_length - 1] == '\n') {
      line_buf[line_length - 1] = '\0';
      line_length--;
    }
    if (line_length > 0 && line_buf[line_length - 1] == '\r') {
      line_buf[line_length - 1] = '\0';
      line_length--;
    }

    Line *new_lines =
        realloc(buffer->lines, (buffer->length + 1) * sizeof(Line));
    if (new_lines == NULL) {
      free(line_buf);
      fclose(f);
      for (size_t i = 0; i < buffer->length; i++) {
        free(buffer->lines[i].data);
      }
      free(buffer->lines);
      free(buffer);
      return NULL;
    }
    buffer->lines = new_lines;

    buffer->lines[buffer->length].data = strdup(line_buf);
    if (buffer->lines[buffer->length].data == NULL) {
      free(line_buf);
      fclose(f);
      for (size_t i = 0; i < buffer->length; i++) {
        free(buffer->lines[i].data);
      }
      free(buffer->lines);
      free(buffer);
      return NULL;
    }
    buffer->lines[buffer->length].length = line_length;
    buffer->length++;
  }

  free(line_buf);
  fclose(f);

  if (buffer->length == 0) {
    buffer->lines = malloc(1 * sizeof(Line));
    if (buffer->lines == NULL) {
      free(buffer);
      return NULL;
    }
    buffer->lines[0].data = strdup("");
    if (buffer->lines[0].data == NULL) {
      free(buffer->lines);
      free(buffer);
      return NULL;
    }
    buffer->lines[0].length = 0;
    buffer->length = 1;
  }

  return buffer;
}

int main(int argc, char *argv[]) {
}
