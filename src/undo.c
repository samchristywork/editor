#include <stdlib.h>
#include <string.h>

#include "undo.h"

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
      state->lines[i].capacity = buffer->lines[i].capacity;
      if (buffer->lines[i].length > 0 && buffer->lines[i].data != NULL) {
        state->lines[i].data = malloc(buffer->lines[i].capacity);
        if (state->lines[i].data != NULL) {
          memcpy(state->lines[i].data, buffer->lines[i].data,
                 buffer->lines[i].length);
        } else {
          state->lines[i].length = 0;
          state->lines[i].capacity = 0;
        }
      } else {
        state->lines[i].data = NULL;
        state->lines[i].capacity = 0;
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
        buffer->lines[i].capacity = state->lines[i].capacity;
        if (state->lines[i].length > 0 && state->lines[i].data != NULL) {
          buffer->lines[i].data = malloc(state->lines[i].capacity);
          if (buffer->lines[i].data != NULL) {
            memcpy(buffer->lines[i].data, state->lines[i].data,
                   state->lines[i].length);
          } else {
            buffer->lines[i].data = NULL;
            buffer->lines[i].length = 0;
            buffer->lines[i].capacity = 0;
          }
        } else {
          buffer->lines[i].data = NULL;
          buffer->lines[i].capacity = 0;
        }
      }
    }
  } else {
    buffer->lines = NULL;
  }

  free_undo_state(state);
}
