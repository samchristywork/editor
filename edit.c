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
      buffer->lines[insert_row + i].data = malloc(len);
      memcpy(buffer->lines[insert_row + i].data, ctx->yank_buffer[i], len);
      buffer->lines[insert_row + i].length = len;
    } else {
      buffer->lines[insert_row + i].data = NULL;
      buffer->lines[insert_row + i].length = 0;
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
    line->data = realloc(line->data, line->length + yank_len);
    memmove(line->data + col + yank_len, line->data + col, line->length - col);
    memcpy(line->data + col, ctx->yank_buffer[0], yank_len);
    line->length += yank_len;
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
    current_line->data = realloc(current_line->data, col);
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
        line->data = realloc(line->data, line->length + yank_len);
        memcpy(line->data + line->length, ctx->yank_buffer[i], yank_len);
        line->length += yank_len;
      } else if (i == ctx->yank_buffer_length - 1) {
        buffer->lines[row + i].data = malloc(yank_len + rest_len);
        memcpy(buffer->lines[row + i].data, ctx->yank_buffer[i], yank_len);
        if (rest_of_line != NULL) {
          memcpy(buffer->lines[row + i].data + yank_len, rest_of_line,
                 rest_len);
        }
        buffer->lines[row + i].length = yank_len + rest_len;
      } else {
        buffer->lines[row + i].data = malloc(yank_len);
        memcpy(buffer->lines[row + i].data, ctx->yank_buffer[i], yank_len);
        buffer->lines[row + i].length = yank_len;
      }
    } else {
      buffer->lines[row + i].data = NULL;
      buffer->lines[row + i].length = 0;
    }
  }

  buffer->length += ctx->yank_buffer_length - 1;
  window->cursor.row += ctx->yank_buffer_length - 1;
  free(rest_of_line);
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

void insert_newline(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  ensure_buffer_initialized(buffer);

  Line *current_line = &buffer->lines[row];
  size_t old_length = current_line->length;
  char *old_data = current_line->data;

  char *new_current_data = NULL;
  if (col > 0) {
    new_current_data = malloc(col);
    if (new_current_data != NULL) {
      memcpy(new_current_data, old_data, col);
    }
  }

  char *new_line_data = NULL;
  size_t new_line_length = old_length - col;
  if (new_line_length > 0) {
    new_line_data = malloc(new_line_length);
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

  buffer->lines[row + 1].data = new_line_data;
  buffer->lines[row + 1].length = new_line_length;

  buffer->length++;

  window->cursor.row++;
  window->cursor.column = 1;
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

  memmove(line->data + col, line->data + col + 1, line->length - col - 1);
  line->length--;
  if (line->length > 0) {
    line->data = realloc(line->data, line->length);
  } else {
    free(line->data);
    line->data = NULL;
  }
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
      prev_line->data =
          realloc(prev_line->data, prev_line->length + line->length);
      memcpy(prev_line->data + prev_line->length, line->data, line->length);
      prev_line->length += line->length;
      free(line->data);
    }

    memmove(&buffer->lines[row], &buffer->lines[row + 1],
            sizeof(Line) * (buffer->length - row - 1));

    buffer->length--;
    buffer->lines = realloc(buffer->lines, sizeof(Line) * buffer->length);

    window->cursor.row--;
    window->cursor.column = prev_length + 1;
  } else if (col > 0 && col <= line->length) {
    memmove(line->data + col - 1, line->data + col, line->length - col);
    line->length--;
    window->cursor.column--;
    if (line->length > 0) {
      line->data = realloc(line->data, line->length);
    } else {
      free(line->data);
      line->data = NULL;
    }
  }
}

static bool is_word_char(char c) {
  return isalnum((unsigned char)c) || c == '_';
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

  size_t delete_count = end - start;
  if (delete_count > 0) {
    memmove(line->data + start, line->data + end, line->length - end);
    line->length -= delete_count;
    if (line->length > 0) {
      line->data = realloc(line->data, line->length);
    } else {
      free(line->data);
      line->data = NULL;
    }
    window->cursor.column = start + 1;
  }
}

void delete_line(Window *window) {
  Buffer *buffer = window->current_buffer;
  size_t row = window->cursor.row - 1;

  if (buffer->length == 0) {
    return;
  }

  Line *line = &buffer->lines[row];

  if (line->data != NULL) {
    free(line->data);
  }

  if (buffer->length == 1) {
    line->data = NULL;
    line->length = 0;
    window->cursor.column = 1;
    return;
  }

  memmove(&buffer->lines[row], &buffer->lines[row + 1],
          sizeof(Line) * (buffer->length - row - 1));

  buffer->length--;
  buffer->lines = realloc(buffer->lines, sizeof(Line) * buffer->length);

  if (row >= buffer->length) {
    window->cursor.row = buffer->length;
  }
  window->cursor.column = 1;
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

void init_undo_stack(Context *ctx) {
  ctx->undo_stack.states = NULL;
  ctx->undo_stack.length = 0;
  ctx->undo_stack.capacity = 0;
}

static void free_undo_state(UndoState *state) {
  if (state->lines != NULL) {
    for (size_t i = 0; i < state->length; i++) {
      free(state->lines[i].data);
    }
    free(state->lines);
    state->lines = NULL;
  }
  state->length = 0;
}

void free_undo_stack(Context *ctx) {
  for (size_t i = 0; i < ctx->undo_stack.length; i++) {
    free_undo_state(&ctx->undo_stack.states[i]);
  }
  free(ctx->undo_stack.states);
  ctx->undo_stack.states = NULL;
  ctx->undo_stack.length = 0;
  ctx->undo_stack.capacity = 0;
}

void push_undo_state(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;

  if (buffer == NULL) {
    return;
  }

  if (ctx->undo_stack.length >= ctx->undo_stack.capacity) {
    size_t new_capacity =
        ctx->undo_stack.capacity == 0 ? 16 : ctx->undo_stack.capacity * 2;
    UndoState *new_states =
        realloc(ctx->undo_stack.states, sizeof(UndoState) * new_capacity);
    if (new_states == NULL) {
      return;
    }
    ctx->undo_stack.states = new_states;
    ctx->undo_stack.capacity = new_capacity;
  }

  UndoState *state = &ctx->undo_stack.states[ctx->undo_stack.length];
  state->length = buffer->length;
  state->cursor = window->cursor;

  if (buffer->length > 0) {
    state->lines = malloc(sizeof(Line) * buffer->length);
    if (state->lines == NULL) {
      return;
    }

    for (size_t i = 0; i < buffer->length; i++) {
      state->lines[i].length = buffer->lines[i].length;
      if (buffer->lines[i].length > 0 && buffer->lines[i].data != NULL) {
        state->lines[i].data = malloc(buffer->lines[i].length);
        if (state->lines[i].data != NULL) {
          memcpy(state->lines[i].data, buffer->lines[i].data,
                 buffer->lines[i].length);
        } else {
          state->lines[i].length = 0;
        }
      } else {
        state->lines[i].data = NULL;
      }
    }
  } else {
    state->lines = NULL;
  }

  ctx->undo_stack.length++;
}

void undo(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;

  if (ctx->undo_stack.length == 0 || buffer == NULL) {
    return;
  }

  ctx->undo_stack.length--;
  UndoState *state = &ctx->undo_stack.states[ctx->undo_stack.length];

  for (size_t i = 0; i < buffer->length; i++) {
    free(buffer->lines[i].data);
  }
  free(buffer->lines);

  buffer->length = state->length;
  window->cursor = state->cursor;

  if (state->length > 0) {
    buffer->lines = malloc(sizeof(Line) * state->length);
    if (buffer->lines != NULL) {
      for (size_t i = 0; i < state->length; i++) {
        buffer->lines[i].length = state->lines[i].length;
        if (state->lines[i].length > 0 && state->lines[i].data != NULL) {
          buffer->lines[i].data = malloc(state->lines[i].length);
          if (buffer->lines[i].data != NULL) {
            memcpy(buffer->lines[i].data, state->lines[i].data,
                   state->lines[i].length);
          } else {
            buffer->lines[i].data = NULL;
            buffer->lines[i].length = 0;
          }
        } else {
          buffer->lines[i].data = NULL;
        }
      }
    }
  } else {
    buffer->lines = NULL;
  }

  free_undo_state(state);
}
