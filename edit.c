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
