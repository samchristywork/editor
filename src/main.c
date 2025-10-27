#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "draw.h"
#include "input.h"
#include "main.h"
#include "undo.h"

Context *global_ctx;

static void enter_alt_screen(void) {
  struct termios raw = global_ctx->terminal.attrs;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  printf("\x1b[?1049h");
  printf("\x1b[?25h");
  printf("\x1b[2J");
  printf("\x1b[H");
  fflush(stdout);
}

static void leave_alt_screen(Context ctx) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx.terminal.attrs);
  printf("\x1b[?1049l");
  printf("\x1b[?25h");
  fflush(stdout);
}

static Buffer *create_buffer_from_file(File file) {
  FILE *f = fopen(file.name, "r");

  Buffer *buffer = malloc(sizeof(Buffer));
  if (buffer == NULL) {
    if (f != NULL) {
      fclose(f);
    }
    return NULL;
  }
  buffer->file = file;
  buffer->lines = NULL;
  buffer->length = 0;

  if (f == NULL) {
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
    return buffer;
  }

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

static void init_buffers(Context *ctx, FileList file_list) {
  ctx->n_buffers = file_list.length;
  ctx->buffers = malloc(file_list.length * sizeof(Buffer *));
  if (ctx->buffers == NULL) {
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < file_list.length; i++) {
    Buffer *b = create_buffer_from_file(file_list.files[i]);
    if (b == NULL) {
      exit(EXIT_FAILURE);
    }
    ctx->buffers[i] = b;
  }
}

static void add_file(FileList *file_list, char *filename) {
  file_list->length++;
  file_list->files =
      realloc(file_list->files, file_list->length * sizeof(File));
  if (file_list->files == NULL) {
    exit(EXIT_FAILURE);
  }

  file_list->files[file_list->length - 1].name = strdup(filename);
  if (file_list->files[file_list->length - 1].name == NULL) {
    exit(EXIT_FAILURE);
  }
}

static void parse_arguments(int argc, char *argv[], Arguments *arguments) {
  arguments->file_list.files = NULL;
  arguments->file_list.length = 0;
  if (argc == 1) {
    add_file(&arguments->file_list, "Untitled");
  } else {
    for (int i = 1; i < argc; i++) {
      add_file(&arguments->file_list, argv[i]);
    }
  }
}

static void get_terminal_size(size_t *width, size_t *height) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    *width = ws.ws_col;
    *height = ws.ws_row;
  }
}

static void handle_sigwinch(int sig) {
  (void)sig;
  get_terminal_size(&global_ctx->terminal.width, &global_ctx->terminal.height);
  draw_screen(global_ctx->windows[global_ctx->current_window],
              global_ctx->terminal.width, global_ctx->terminal.height,
              global_ctx->mode, &global_ctx->selection,
              global_ctx->command_buffer, global_ctx->command_buffer_length,
              global_ctx->search_buffer, global_ctx->search_buffer_length,
              global_ctx->show_line_numbers);
}

static void handle_sigint(int sig) {
  (void)sig;
  global_ctx->running = false;
}

static void init_terminal(struct termios *attr) {
  tcgetattr(STDIN_FILENO, attr);
  signal(SIGWINCH, handle_sigwinch);
  signal(SIGINT, handle_sigint);
  enter_alt_screen();
}

static void add_window(Context *ctx, size_t current_buffer) {
  (void)current_buffer;
  ctx->n_windows++;
  ctx->windows = realloc(ctx->windows, ctx->n_windows * sizeof(Window *));
  ctx->windows[ctx->n_windows - 1] = malloc(sizeof(Window));
  Window *window = ctx->windows[ctx->n_windows - 1];
  if (window == NULL) {
    exit(EXIT_FAILURE);
  }
  window->row = 2;
  window->column = 2;
  window->width = 10;
  window->height = 10;
  window->cursor.row = 1;
  window->cursor.column = 1;
  window->scroll.horizontal = 0;
  window->scroll.vertical = 0;
  window->current_buffer = ctx->buffers[0];
}

static void cleanup(Context ctx, Arguments arguments) {
  for (size_t i = 0; i < ctx.n_buffers; i++) {
    for (size_t j = 0; j < ctx.buffers[i]->length; j++) {
      free(ctx.buffers[i]->lines[j].data);
    }
    free(ctx.buffers[i]->lines);
    free(ctx.buffers[i]);
  }
  free(ctx.buffers);
  for (size_t i = 0; i < ctx.n_windows; i++) {
    free(ctx.windows[i]);
  }
  free(ctx.windows);
  for (size_t i = 0; i < arguments.file_list.length; i++) {
    free(arguments.file_list.files[i].name);
  }
  free(arguments.file_list.files);
  free(ctx.command_buffer);
  free(ctx.search_buffer);
  if (ctx.yank_buffer != NULL) {
    for (size_t i = 0; i < ctx.yank_buffer_length; i++) {
      free(ctx.yank_buffer[i]);
    }
    free(ctx.yank_buffer);
    free(ctx.yank_buffer_lengths);
  }
  free_undo_stack(&ctx);
  leave_alt_screen(ctx);
}

int main(int argc, char *argv[]) {
  Context ctx = {.running = true, .mode = MODE_NORMAL};
  ctx.buffers = NULL;
  ctx.windows = NULL;
  ctx.n_buffers = 0;
  ctx.n_windows = 0;
  ctx.current_window = 0;
  ctx.command_buffer = NULL;
  ctx.command_buffer_length = 0;
  ctx.search_buffer = NULL;
  ctx.search_buffer_length = 0;
  ctx.yank_buffer = NULL;
  ctx.yank_buffer_lengths = NULL;
  ctx.yank_buffer_length = 0;
  ctx.yank_linewise = false;
  global_ctx = &ctx;

  Arguments arguments = {0};
  parse_arguments(argc, argv, &arguments);

  init_terminal(&ctx.terminal.attrs);

  init_buffers(&ctx, arguments.file_list);
  add_window(&ctx, 0);
  init_undo_stack(&ctx);
  ctx.show_line_numbers = true;

  handle_sigwinch(0);
  draw_screen(ctx.windows[ctx.current_window], ctx.terminal.width,
              ctx.terminal.height, ctx.mode, &ctx.selection, ctx.command_buffer,
              ctx.command_buffer_length, ctx.search_buffer,
              ctx.search_buffer_length, ctx.show_line_numbers);
  while (ctx.running) {
    handle_input(&ctx);
  }
  cleanup(ctx, arguments);
}
