#include <stdlib.h>
#include <string.h>

#include "yank.h"

static void free_yank_buffer(Context *ctx) {
  if (ctx->yank_buffer != NULL) {
    for (size_t i = 0; i < ctx->yank_buffer_length; i++) {
      free(ctx->yank_buffer[i]);
    }
    free(ctx->yank_buffer);
    free(ctx->yank_buffer_lengths);
    ctx->yank_buffer = NULL;
    ctx->yank_buffer_lengths = NULL;
    ctx->yank_buffer_length = 0;
  }
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

static void copy_line_to_yank(Buffer *buffer, size_t row, char **dest,
                              size_t *dest_len) {
  if (row < buffer->length) {
    Line *line = &buffer->lines[row];
    if (line->length > 0) {
      *dest = malloc(line->length);
      if (*dest != NULL) {
        memcpy(*dest, line->data, line->length);
        *dest_len = line->length;
      } else {
        *dest_len = 0;
      }
    } else {
      *dest = NULL;
      *dest_len = 0;
    }
  } else {
    *dest = NULL;
    *dest_len = 0;
  }
}

static void yank_linewise(Context *ctx, Buffer *buffer, size_t start_row,
                          size_t end_row) {
  ctx->yank_linewise = true;
  ctx->yank_buffer_length = end_row - start_row + 1;
  ctx->yank_buffer = malloc(sizeof(char *) * ctx->yank_buffer_length);
  ctx->yank_buffer_lengths = malloc(sizeof(size_t) * ctx->yank_buffer_length);

  for (size_t i = 0; i < ctx->yank_buffer_length; i++) {
    size_t row = start_row + i - 1;
    copy_line_to_yank(buffer, row, &ctx->yank_buffer[i],
                      &ctx->yank_buffer_lengths[i]);
  }
}

static void yank_single_line(Context *ctx, Buffer *buffer, size_t row,
                             size_t start_col, size_t end_col) {
  ctx->yank_buffer_length = 1;
  ctx->yank_buffer = malloc(sizeof(char *));
  ctx->yank_buffer_lengths = malloc(sizeof(size_t));

  if (row - 1 < buffer->length) {
    Line *line = &buffer->lines[row - 1];
    size_t len = end_col - start_col + 1;
    if (start_col - 1 < line->length) {
      if (end_col > line->length) {
        len = line->length - start_col + 1;
      }
      ctx->yank_buffer[0] = malloc(len);
      if (ctx->yank_buffer[0] != NULL) {
        memcpy(ctx->yank_buffer[0], line->data + start_col - 1, len);
        ctx->yank_buffer_lengths[0] = len;
      } else {
        ctx->yank_buffer_lengths[0] = 0;
      }
    } else {
      ctx->yank_buffer[0] = NULL;
      ctx->yank_buffer_lengths[0] = 0;
    }
  } else {
    ctx->yank_buffer[0] = NULL;
    ctx->yank_buffer_lengths[0] = 0;
  }
}

static void yank_multiple_lines(Context *ctx, Buffer *buffer, size_t start_row,
                                size_t start_col, size_t end_row,
                                size_t end_col) {
  ctx->yank_buffer_length = end_row - start_row + 1;
  ctx->yank_buffer = malloc(sizeof(char *) * ctx->yank_buffer_length);
  ctx->yank_buffer_lengths = malloc(sizeof(size_t) * ctx->yank_buffer_length);

  for (size_t i = 0; i < ctx->yank_buffer_length; i++) {
    size_t row = start_row + i - 1;
    if (row < buffer->length) {
      Line *line = &buffer->lines[row];
      if (i == 0) {
        size_t len = line->length - start_col + 1;
        if (start_col - 1 < line->length) {
          ctx->yank_buffer[i] = malloc(len);
          if (ctx->yank_buffer[i] != NULL) {
            memcpy(ctx->yank_buffer[i], line->data + start_col - 1, len);
            ctx->yank_buffer_lengths[i] = len;
          } else {
            ctx->yank_buffer_lengths[i] = 0;
          }
        } else {
          ctx->yank_buffer[i] = NULL;
          ctx->yank_buffer_lengths[i] = 0;
        }
      } else if (i == ctx->yank_buffer_length - 1) {
        size_t len = end_col;
        if (end_col > line->length) {
          len = line->length;
        }
        if (len > 0) {
          ctx->yank_buffer[i] = malloc(len);
          if (ctx->yank_buffer[i] != NULL) {
            memcpy(ctx->yank_buffer[i], line->data, len);
            ctx->yank_buffer_lengths[i] = len;
          } else {
            ctx->yank_buffer_lengths[i] = 0;
          }
        } else {
          ctx->yank_buffer[i] = NULL;
          ctx->yank_buffer_lengths[i] = 0;
        }
      } else {
        copy_line_to_yank(buffer, row, &ctx->yank_buffer[i],
                          &ctx->yank_buffer_lengths[i]);
      }
    } else {
      ctx->yank_buffer[i] = NULL;
      ctx->yank_buffer_lengths[i] = 0;
    }
  }
}

static void paste_linewise(Context *ctx, Window *window, Buffer *buffer) {
  size_t insert_row = window->cursor.row;

  buffer->lines = realloc(
      buffer->lines, sizeof(Line) * (buffer->length + ctx->yank_buffer_length));
  memmove(&buffer->lines[insert_row + ctx->yank_buffer_length],
          &buffer->lines[insert_row],
          sizeof(Line) * (buffer->length - insert_row));

  for (size_t i = 0; i < ctx->yank_buffer_length; i++) {
    size_t len = ctx->yank_buffer_lengths[i];
    if (ctx->yank_buffer[i] != NULL && len > 0) {
      size_t capacity = len < 16 ? 16 : len * 2;
      buffer->lines[insert_row + i].data = malloc(capacity);
      memcpy(buffer->lines[insert_row + i].data, ctx->yank_buffer[i], len);
      buffer->lines[insert_row + i].length = len;
      buffer->lines[insert_row + i].capacity = capacity;
    } else {
      buffer->lines[insert_row + i].data = NULL;
      buffer->lines[insert_row + i].length = 0;
      buffer->lines[insert_row + i].capacity = 0;
    }
  }

  buffer->length += ctx->yank_buffer_length;
  window->cursor.row = insert_row + 1;
  window->cursor.column = 1;
}

static void paste_single_line(Context *ctx, Window *window, Buffer *buffer) {
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;
  size_t yank_len = ctx->yank_buffer_lengths[0];

  if (ctx->yank_buffer[0] != NULL && yank_len > 0) {
    Line *line = &buffer->lines[row];
    size_t new_length = line->length + yank_len;
    if (new_length > line->capacity) {
      size_t new_capacity = line->capacity == 0 ? 16 : line->capacity;
      while (new_capacity < new_length) {
        new_capacity *= 2;
      }
      line->data = realloc(line->data, new_capacity);
      line->capacity = new_capacity;
    }
    memmove(line->data + col + yank_len, line->data + col, line->length - col);
    memcpy(line->data + col, ctx->yank_buffer[0], yank_len);
    line->length = new_length;
    window->cursor.column += yank_len;
  }
}

static void paste_multiple_lines(Context *ctx, Window *window, Buffer *buffer) {
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;
  Line *current_line = &buffer->lines[row];

  char *rest_of_line = NULL;
  size_t rest_len = 0;
  if (col < current_line->length) {
    rest_len = current_line->length - col;
    rest_of_line = malloc(rest_len);
    if (rest_of_line != NULL) {
      memcpy(rest_of_line, current_line->data + col, rest_len);
    }
    current_line->length = col;
    if (col > 0) {
      current_line->data = realloc(current_line->data, col);
    } else {
      free(current_line->data);
      current_line->data = NULL;
    }
  }

  buffer->lines = realloc(
      buffer->lines, sizeof(Line) * (buffer->length + ctx->yank_buffer_length));
  memmove(&buffer->lines[row + ctx->yank_buffer_length],
          &buffer->lines[row + 1], sizeof(Line) * (buffer->length - row - 1));

  for (size_t i = 0; i < ctx->yank_buffer_length; i++) {
    size_t yank_len = ctx->yank_buffer_lengths[i];
    if (ctx->yank_buffer[i] != NULL && yank_len > 0) {
      if (i == 0) {
        Line *line = &buffer->lines[row];
        size_t new_length = line->length + yank_len;
        if (new_length > line->capacity) {
          size_t new_capacity = line->capacity == 0 ? 16 : line->capacity;
          while (new_capacity < new_length) {
            new_capacity *= 2;
          }
          line->data = realloc(line->data, new_capacity);
          line->capacity = new_capacity;
        }
        memcpy(line->data + line->length, ctx->yank_buffer[i], yank_len);
        line->length = new_length;
      } else if (i == ctx->yank_buffer_length - 1) {
        size_t new_length = yank_len + rest_len;
        size_t capacity = new_length < 16 ? 16 : new_length * 2;
        buffer->lines[row + i].data = malloc(capacity);
        memcpy(buffer->lines[row + i].data, ctx->yank_buffer[i], yank_len);
        if (rest_of_line != NULL) {
          memcpy(buffer->lines[row + i].data + yank_len, rest_of_line,
                 rest_len);
        }
        buffer->lines[row + i].length = new_length;
        buffer->lines[row + i].capacity = capacity;
      } else {
        size_t capacity = yank_len < 16 ? 16 : yank_len * 2;
        buffer->lines[row + i].data = malloc(capacity);
        memcpy(buffer->lines[row + i].data, ctx->yank_buffer[i], yank_len);
        buffer->lines[row + i].length = yank_len;
        buffer->lines[row + i].capacity = capacity;
      }
    } else {
      buffer->lines[row + i].data = NULL;
      buffer->lines[row + i].length = 0;
      buffer->lines[row + i].capacity = 0;
    }
  }

  buffer->length += ctx->yank_buffer_length - 1;
  window->cursor.row += ctx->yank_buffer_length - 1;
  free(rest_of_line);
}

void yank_selection(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;
  Selection *sel = &ctx->selection;

  free_yank_buffer(ctx);

  size_t start_row = sel->start.row;
  size_t start_col = sel->start.column;
  size_t end_row = sel->end.row;
  size_t end_col = sel->end.column;

  normalize_selection(&start_row, &start_col, &end_row, &end_col);

  if (ctx->mode == MODE_LINEWISE_VISUAL) {
    yank_linewise(ctx, buffer, start_row, end_row);
  } else if (ctx->mode == MODE_CHARACTERWISE_VISUAL) {
    ctx->yank_linewise = false;
    if (start_row == end_row) {
      yank_single_line(ctx, buffer, start_row, start_col, end_col);
    } else {
      yank_multiple_lines(ctx, buffer, start_row, start_col, end_row, end_col);
    }
  }
}

void yank_current_line(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;

  free_yank_buffer(ctx);

  size_t current_row = window->cursor.row;
  yank_linewise(ctx, buffer, current_row, current_row);
}

void paste_buffer(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;

  if (ctx->yank_buffer == NULL || ctx->yank_buffer_length == 0) {
    return;
  }

  if (ctx->yank_linewise) {
    paste_linewise(ctx, window, buffer);
  } else {
    if (ctx->yank_buffer_length == 1) {
      paste_single_line(ctx, window, buffer);
    } else {
      paste_multiple_lines(ctx, window, buffer);
    }
  }
}
