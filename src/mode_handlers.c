#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "delete.h"
#include "insert.h"
#include "main.h"
#include "mode_handlers.h"
#include "save.h"
#include "search.h"
#include "text_objects.h"
#include "undo.h"
#include "yank.h"

void handle_normal_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;
  char **search_buffer = &ctx->search_buffer;
  size_t *search_buffer_length = &ctx->search_buffer_length;
  size_t repeat_count = ctx->count > 0 ? ctx->count : 1;

  if (isdigit(c)) {
    if (c != '0' || ctx->count > 0) {
      ctx->count = ctx->count * 10 + (c - '0');
      return;
    }
  }

  switch (c) {
  case 'h':
    for (size_t i = 0; i < repeat_count; i++) {
      window->cursor.column--;
    }
    break;
  case 'j':
    for (size_t i = 0; i < repeat_count; i++) {
      window->cursor.row++;
    }
    break;
  case 'k':
    for (size_t i = 0; i < repeat_count; i++) {
      window->cursor.row--;
    }
    break;
  case 'l':
    for (size_t i = 0; i < repeat_count; i++) {
      window->cursor.column++;
    }
    break;
  case '0':
    window->cursor.column = 1;
    break;
  case '$': {
    Line *line_ptr = &window->current_buffer->lines[window->cursor.row - 1];
    window->cursor.column = line_ptr->length + 1;
    break;
  }
  case 4:
    window->cursor.row += window->height / 2;
    break;
  case 21:
    if (window->cursor.row < window->height / 2) {
      window->cursor.row = 0;
    } else {
      window->cursor.row -= window->height / 2;
    }
    break;
  case 'g':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'g') {
        window->cursor.row = 1;
      }
    }
    break;
  case 'G':
    window->cursor.row = window->current_buffer->length;
    break;
  case 'o': {
    push_undo_state(ctx);
    Line *line_ptr = &window->current_buffer->lines[window->cursor.row - 1];
    size_t leading_spaces = 0;
    if (line_ptr->data != NULL) {
      for (; leading_spaces < line_ptr->length; leading_spaces++) {
        if (line_ptr->data[leading_spaces] != ' ') {
          break;
        }
      }
    }
    window->cursor.column = line_ptr->length + 1;
    insert_newline(window);
    for (size_t i = 0; i < leading_spaces; i++) {
      insert_char(window, ' ');
    }
    *mode = MODE_INSERT;
    break;
  }
  case 'x':
    push_undo_state(ctx);
    for (size_t i = 0; i < repeat_count; i++) {
      delete_char(window);
    }
    break;
  case 'd':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'd') {
        push_undo_state(ctx);
        for (size_t i = 0; i < repeat_count; i++) {
          delete_line(window);
        }
      } else if (c == 'w') {
        push_undo_state(ctx);
        for (size_t i = 0; i < repeat_count; i++) {
          delete_word(window);
        }
      } else if (c == 'i') {
        unsigned char text_obj;
        if (read(STDIN_FILENO, &text_obj, 1) == 1) {
          size_t start_row, start_col, end_row, end_col;
          if (find_text_object(window, text_obj, &start_row, &start_col,
                               &end_row, &end_col)) {
            push_undo_state(ctx);
            window->cursor.row = start_row + 1;
            window->cursor.column = start_col + 1;
            delete_range(window, start_row, start_col, end_row, end_col);
          }
        }
      }
    }
    break;
  case 'y':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'y') {
        yank_current_line(ctx);
      }
    }
    break;
  case 'c':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'i') {
        unsigned char text_obj;
        if (read(STDIN_FILENO, &text_obj, 1) == 1) {
          size_t start_row, start_col, end_row, end_col;
          if (find_text_object(window, text_obj, &start_row, &start_col,
                               &end_row, &end_col)) {
            push_undo_state(ctx);
            window->cursor.row = start_row + 1;
            window->cursor.column = start_col + 1;
            delete_range(window, start_row, start_col, end_row, end_col);
            *mode = MODE_INSERT;
          }
        }
      } else if (c == 'w') {
        push_undo_state(ctx);
        delete_word(window);
        *mode = MODE_INSERT;
      }
    }
    break;
  case 'i':
    push_undo_state(ctx);
    *mode = MODE_INSERT;
    break;
  case 'I': {
    push_undo_state(ctx);
    Line *line_ptr = &window->current_buffer->lines[window->cursor.row - 1];
    if (line_ptr->data != NULL) {
      for (window->cursor.column = 0;
           line_ptr->data[window->cursor.column] == ' ' &&
           window->cursor.column < line_ptr->length;
           window->cursor.column++) {
      }
    } else {
      window->cursor.column = 0;
    }
    window->cursor.column++;
    *mode = MODE_INSERT;
    break;
  }
  case 'A': {
    push_undo_state(ctx);
    Line *line_ptr = &window->current_buffer->lines[window->cursor.row - 1];
    window->cursor.column = line_ptr->length + 1;
    *mode = MODE_INSERT;
    break;
  }
  case ':':
    *mode = MODE_COMMAND;
    break;
  case '/':
    free(*search_buffer);
    *search_buffer = NULL;
    *search_buffer_length = 0;
    ctx->search_buffer_capacity = 0;
    *mode = MODE_SEARCH;
    break;
  case 'p':
    push_undo_state(ctx);
    paste_buffer(ctx);
    break;
  case 'V':
    *mode = MODE_LINEWISE_VISUAL;
    ctx->selection.start.row = window->cursor.row;
    ctx->selection.start.column = window->cursor.column;
    ctx->selection.end.row = window->cursor.row;
    ctx->selection.end.column = window->cursor.column;
    break;
  case 'v':
    *mode = MODE_CHARACTERWISE_VISUAL;
    ctx->selection.start.row = window->cursor.row;
    ctx->selection.start.column = window->cursor.column;
    ctx->selection.end.row = window->cursor.row;
    ctx->selection.end.column = window->cursor.column;
    break;
  case 'u':
    undo(ctx);
    break;
  case 'n':
    if (*search_buffer_length > 0) {
      find_occurrence(window, *search_buffer, *search_buffer_length,
                      SEARCH_FORWARD);
    }
    break;
  case 'N':
    if (*search_buffer_length > 0) {
      find_occurrence(window, *search_buffer, *search_buffer_length,
                      SEARCH_BACKWARD);
    }
    break;
  case ']':
    ctx->show_line_numbers = !ctx->show_line_numbers;
    break;
  case 'z':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'z') {
        size_t target_row = window->cursor.row;
        size_t half_height = window->height / 2;

        if (target_row > half_height) {
          window->scroll.vertical = target_row - half_height - 1;
        } else {
          window->scroll.vertical = 0;
        }
      }
    }
    break;
  }

  ctx->count = 0;
}

static size_t find_current_buffer_index(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  for (size_t i = 0; i < ctx->n_buffers; i++) {
    if (ctx->buffers[i] == window->current_buffer) {
      return i;
    }
  }
  return 0;
}

static void switch_to_buffer(Context *ctx, size_t buffer_idx) {
  Window *window = ctx->windows[ctx->current_window];
  window->current_buffer = ctx->buffers[buffer_idx];
  window->cursor.row = 1;
  window->cursor.column = 1;
  window->scroll.vertical = 0;
  window->scroll.horizontal = 0;
}

static void command_next_buffer(Context *ctx) {
  size_t current_idx = find_current_buffer_index(ctx);
  size_t next_idx = (current_idx + 1) % ctx->n_buffers;
  switch_to_buffer(ctx, next_idx);
}

static void command_prev_buffer(Context *ctx) {
  size_t current_idx = find_current_buffer_index(ctx);
  size_t prev_idx = (current_idx == 0) ? ctx->n_buffers - 1 : current_idx - 1;
  switch_to_buffer(ctx, prev_idx);
}

static void command_goto_line(Context *ctx, char *command_buffer,
                              size_t command_buffer_length) {
  Window *window = ctx->windows[ctx->current_window];
  char *temp = realloc(command_buffer, command_buffer_length + 1);
  if (temp != NULL) {
    ctx->command_buffer = temp;
    command_buffer = temp;
    command_buffer[command_buffer_length] = '\0';
  }
  int line_number_int = atoi(command_buffer);
  size_t line_number = line_number_int < 0 ? 0 : (size_t)line_number_int;
  if (line_number > window->current_buffer->length) {
    line_number = window->current_buffer->length;
  }
  window->cursor.row = line_number;
  window->cursor.column = 1;
}

static bool is_numeric_command(char *command_buffer,
                               size_t command_buffer_length) {
  for (size_t i = 0; i < command_buffer_length; i++) {
    if (!isdigit(command_buffer[i])) {
      return false;
    }
  }
  return true;
}

static bool command_matches(char *command_buffer, size_t command_buffer_length,
                            const char *command) {
  size_t command_len = strlen(command);
  return command_buffer_length == command_len &&
         strncmp(command_buffer, command, command_len) == 0;
}

static void execute_command(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  char *command_buffer = ctx->command_buffer;
  size_t command_buffer_length = ctx->command_buffer_length;

  if (command_matches(command_buffer, command_buffer_length, "w")) {
    save_buffer(window->current_buffer);
  } else if (command_matches(command_buffer, command_buffer_length, "q")) {
    ctx->running = false;
  } else if (command_matches(command_buffer, command_buffer_length, "x")) {
    save_buffer(window->current_buffer);
    ctx->running = false;
  } else if (command_matches(command_buffer, command_buffer_length, "bn")) {
    command_next_buffer(ctx);
  } else if (command_matches(command_buffer, command_buffer_length, "bp")) {
    command_prev_buffer(ctx);
  } else if (command_buffer_length > 0 &&
             is_numeric_command(command_buffer, command_buffer_length)) {
    command_goto_line(ctx, command_buffer, command_buffer_length);
  }
}

void handle_command_mode(Context *ctx, unsigned char c) {
  EditorMode *mode = &ctx->mode;
  char **command_buffer = &ctx->command_buffer;
  size_t *command_buffer_length = &ctx->command_buffer_length;

  switch (c) {
  case 27:
    *mode = MODE_NORMAL;
    free(*command_buffer);
    *command_buffer = NULL;
    *command_buffer_length = 0;
    break;
  case '\r':
  case '\n':
    execute_command(ctx);
    *mode = MODE_NORMAL;
    free(*command_buffer);
    *command_buffer = NULL;
    *command_buffer_length = 0;
    ctx->command_buffer_capacity = 0;
    break;
  case 127:
  case 8:
    if (*command_buffer_length > 0) {
      (*command_buffer_length)--;
      if (*command_buffer_length == 0) {
        free(*command_buffer);
        *command_buffer = NULL;
        ctx->command_buffer_capacity = 0;
      }
    }
    break;
  default:
    if (c >= 32 && c <= 126) {
      if (*command_buffer_length >= ctx->command_buffer_capacity) {
        size_t new_capacity = ctx->command_buffer_capacity == 0 ? 16 : ctx->command_buffer_capacity * 2;
        char *temp = realloc(*command_buffer, new_capacity);
        if (temp != NULL) {
          *command_buffer = temp;
          ctx->command_buffer_capacity = new_capacity;
        } else {
          break;
        }
      }
      (*command_buffer)[*command_buffer_length] = c;
      (*command_buffer_length)++;
    }
    break;
  }
}

void handle_search_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;
  char **search_buffer = &ctx->search_buffer;
  size_t *search_buffer_length = &ctx->search_buffer_length;

  switch (c) {
  case 27:
    *mode = MODE_NORMAL;
    free(*search_buffer);
    *search_buffer = NULL;
    *search_buffer_length = 0;
    break;
  case '\r':
  case '\n':
    if (*search_buffer_length > 0) {
      find_occurrence(window, *search_buffer, *search_buffer_length,
                      SEARCH_FORWARD);
    }
    *mode = MODE_NORMAL;
    break;
  case 127:
  case 8:
    if (*search_buffer_length > 0) {
      (*search_buffer_length)--;
      if (*search_buffer_length == 0) {
        free(*search_buffer);
        *search_buffer = NULL;
        ctx->search_buffer_capacity = 0;
      }
    }
    break;
  default:
    if (c >= 32 && c <= 126) {
      if (*search_buffer_length >= ctx->search_buffer_capacity) {
        size_t new_capacity = ctx->search_buffer_capacity == 0 ? 16 : ctx->search_buffer_capacity * 2;
        char *temp = realloc(*search_buffer, new_capacity);
        if (temp != NULL) {
          *search_buffer = temp;
          ctx->search_buffer_capacity = new_capacity;
        } else {
          break;
        }
      }
      (*search_buffer)[*search_buffer_length] = c;
      (*search_buffer_length)++;
    }
    break;
  }
}

static void execute_filter(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Buffer *buffer = window->current_buffer;
  Selection *sel = &ctx->selection;
  char *filter_buffer = ctx->filter_buffer;
  size_t filter_buffer_length = ctx->filter_buffer_length;

  if (filter_buffer_length == 0) {
    return;
  }

  char *command = malloc(filter_buffer_length + 1);
  if (command == NULL) {
    return;
  }
  memcpy(command, filter_buffer, filter_buffer_length);
  command[filter_buffer_length] = '\0';

  size_t start_row = sel->start.row;
  size_t end_row = sel->end.row;
  if (start_row > end_row) {
    size_t temp = start_row;
    start_row = end_row;
    end_row = temp;
  }

  size_t num_lines = end_row - start_row + 1;
  size_t total_size = 0;
  for (size_t i = start_row - 1; i < end_row && i < buffer->length; i++) {
    total_size += buffer->lines[i].length + 1;
  }

  char *input_text = malloc(total_size);
  if (input_text == NULL) {
    free(command);
    return;
  }

  size_t offset = 0;
  for (size_t i = start_row - 1; i < end_row && i < buffer->length; i++) {
    if (buffer->lines[i].length > 0 && buffer->lines[i].data != NULL) {
      memcpy(input_text + offset, buffer->lines[i].data, buffer->lines[i].length);
      offset += buffer->lines[i].length;
    }
    input_text[offset++] = '\n';
  }

  int pipe_in[2], pipe_out[2];
  if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
    free(command);
    free(input_text);
    return;
  }

  pid_t pid = fork();
  if (pid == -1) {
    free(command);
    free(input_text);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return;
  }

  if (pid == 0) {
    close(pipe_in[1]);
    close(pipe_out[0]);
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    close(pipe_in[0]);
    close(pipe_out[1]);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(1);
  }

  close(pipe_in[0]);
  close(pipe_out[1]);

  write(pipe_in[1], input_text, offset);
  close(pipe_in[1]);
  free(input_text);

  char *output = NULL;
  size_t output_size = 0;
  size_t output_capacity = 0;
  char read_buffer[4096];
  ssize_t bytes_read;

  while ((bytes_read = read(pipe_out[0], read_buffer, sizeof(read_buffer))) > 0) {
    if (output_size + bytes_read > output_capacity) {
      size_t new_capacity = output_capacity == 0 ? 4096 : output_capacity * 2;
      while (output_size + bytes_read > new_capacity) {
        new_capacity *= 2;
      }
      char *new_output = realloc(output, new_capacity);
      if (new_output == NULL) {
        free(output);
        free(command);
        close(pipe_out[0]);
        return;
      }
      output = new_output;
      output_capacity = new_capacity;
    }
    memcpy(output + output_size, read_buffer, bytes_read);
    output_size += bytes_read;
  }

  close(pipe_out[0]);
  free(command);

  waitpid(pid, NULL, 0);

  push_undo_state(ctx);
  window->cursor.row = start_row;
  window->cursor.column = 1;
  for (size_t i = 0; i < num_lines; i++) {
    delete_line(window);
    if (window->cursor.row > start_row) {
      window->cursor.row--;
    }
  }

  if (buffer->length == 0) {
    free(output);
    return;
  }

  if (window->cursor.row > buffer->length) {
    window->cursor.row = buffer->length;
  }
  if (window->cursor.row < 1) {
    window->cursor.row = 1;
  }

  if (start_row <= buffer->length) {
    window->cursor.row = start_row;
  } else {
    window->cursor.row = buffer->length;
  }
  window->cursor.column = 1;

  if (output_size > 0) {
    for (size_t i = 0; i < output_size; i++) {
      if (output[i] == '\n') {
        insert_newline(window);
      } else {
        insert_char(window, output[i]);
      }
    }
  }

  free(output);
}

void handle_filter_mode(Context *ctx, unsigned char c) {
  EditorMode *mode = &ctx->mode;
  char **filter_buffer = &ctx->filter_buffer;
  size_t *filter_buffer_length = &ctx->filter_buffer_length;

  switch (c) {
  case 27:
    *mode = MODE_LINEWISE_VISUAL;
    free(*filter_buffer);
    *filter_buffer = NULL;
    *filter_buffer_length = 0;
    ctx->filter_buffer_capacity = 0;
    break;
  case '\r':
  case '\n':
    execute_filter(ctx);
    *mode = MODE_NORMAL;
    free(*filter_buffer);
    *filter_buffer = NULL;
    *filter_buffer_length = 0;
    ctx->filter_buffer_capacity = 0;
    break;
  case 127:
  case 8:
    if (*filter_buffer_length > 0) {
      (*filter_buffer_length)--;
      if (*filter_buffer_length == 0) {
        free(*filter_buffer);
        *filter_buffer = NULL;
        ctx->filter_buffer_capacity = 0;
      }
    }
    break;
  default:
    if (c >= 32 && c <= 126) {
      if (*filter_buffer_length >= ctx->filter_buffer_capacity) {
        size_t new_capacity = ctx->filter_buffer_capacity == 0 ? 16 : ctx->filter_buffer_capacity * 2;
        char *temp = realloc(*filter_buffer, new_capacity);
        if (temp != NULL) {
          *filter_buffer = temp;
          ctx->filter_buffer_capacity = new_capacity;
        } else {
          break;
        }
      }
      (*filter_buffer)[*filter_buffer_length] = c;
      (*filter_buffer_length)++;
    }
    break;
  }
}

void handle_insert_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;

  switch (c) {
  case 27: {
    *mode = MODE_NORMAL;
    break;
  }
  case '\t':
    insert_char(window, ' ');
    insert_char(window, ' ');
    break;
  case 127:
  case 8:
    backspace_char(window);
    break;
  case '\r':
  case '\n':
    insert_newline(window);
    break;
  default:
    if (c >= 32 && c <= 126) {
      insert_char(window, c);
    }
    break;
  }
}

static void move_cursor_to_selection_end(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  Selection *sel = &ctx->selection;

  if (sel->end.row > sel->start.row ||
      (sel->end.row == sel->start.row && sel->end.column >= sel->start.column)) {
    window->cursor.row = sel->end.row;
    window->cursor.column = sel->end.column;
  } else {
    window->cursor.row = sel->start.row;
    window->cursor.column = sel->start.column;
  }
}

void handle_visual_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;
  bool update_selection_end = true;

  switch (c) {
  case 27:
    move_cursor_to_selection_end(ctx);
    *mode = MODE_NORMAL;
    break;
  case 'h':
    window->cursor.column--;
    break;
  case 'j':
    window->cursor.row++;
    break;
  case 'k':
    window->cursor.row--;
    break;
  case 'l':
    window->cursor.column++;
    break;
  case '0':
    window->cursor.column = 1;
    break;
  case '$': {
    Line *line_ptr = &window->current_buffer->lines[window->cursor.row - 1];
    window->cursor.column = line_ptr->length + 1;
    break;
  }
  case 'g':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'g') {
        window->cursor.row = 1;
      }
    }
    break;
  case 'G':
    window->cursor.row = window->current_buffer->length;
    break;
  case 'y':
    yank_selection(ctx);
    move_cursor_to_selection_end(ctx);
    *mode = MODE_NORMAL;
    break;
  case 'd':
    push_undo_state(ctx);
    delete_selection(ctx);
    move_cursor_to_selection_end(ctx);
    *mode = MODE_NORMAL;
    break;
  case 'f':
    if (*mode == MODE_LINEWISE_VISUAL) {
      free(ctx->filter_buffer);
      ctx->filter_buffer = NULL;
      ctx->filter_buffer_length = 0;
      ctx->filter_buffer_capacity = 0;
      *mode = MODE_FILTER;
    }
    break;
  case 'i':
    if (*mode == MODE_CHARACTERWISE_VISUAL) {
      unsigned char text_obj;
      if (read(STDIN_FILENO, &text_obj, 1) == 1) {
        Buffer *buf = window->current_buffer;
        size_t row = window->cursor.row - 1;
        size_t col = window->cursor.column - 1;

        if (row < buf->length) {
          Line *line = &buf->lines[row];

          if (text_obj == 'p') {
            size_t para_start = row;
            size_t para_end = row;

            for (size_t r = row; r > 0; r--) {
              Line *check_line = &buf->lines[r - 1];
              if (check_line->length == 0) {
                break;
              }
              para_start = r - 1;
            }

            for (size_t r = row + 1; r < buf->length; r++) {
              Line *check_line = &buf->lines[r];
              if (check_line->length == 0) {
                break;
              }
              para_end = r;
            }

            ctx->selection.start.row = para_start + 1;
            ctx->selection.start.column = 1;
            ctx->selection.end.row = para_end + 1;
            ctx->selection.end.column = buf->lines[para_end].length + 1;
            update_selection_end = false;
          } else if (text_obj == 'w') {
            if (col < line->length && isalnum((unsigned char)line->data[col])) {
              size_t start = col;
              size_t end = col;

              while (start > 0 &&
                     isalnum((unsigned char)line->data[start - 1])) {
                start--;
              }
              while (end < line->length &&
                     isalnum((unsigned char)line->data[end])) {
                end++;
              }

              ctx->selection.start.row = window->cursor.row;
              ctx->selection.start.column = start + 1;
              ctx->selection.end.row = window->cursor.row;
              ctx->selection.end.column = end;
              update_selection_end = false;
            }
          } else if (text_obj == '"' || text_obj == '\'' || text_obj == '<' ||
                     text_obj == '{' || text_obj == '[' || text_obj == '(') {
            char close_char;
            bool is_bracket = (text_obj == '<' || text_obj == '{' ||
                               text_obj == '[' || text_obj == '(');
            if (text_obj == '<')
              close_char = '>';
            else if (text_obj == '{')
              close_char = '}';
            else if (text_obj == '[')
              close_char = ']';
            else if (text_obj == '(')
              close_char = ')';
            else
              close_char = text_obj;

            size_t start_row = row;
            size_t start_col = 0;
            size_t end_row = row;
            size_t end_col = 0;
            bool found_start = false;
            bool found_end = false;

            if (is_bracket) {
              int depth = 0;
              for (size_t i = col; i > 0; i--) {
                if (line->data[i - 1] == close_char) {
                  depth++;
                } else if (line->data[i - 1] == text_obj) {
                  if (depth == 0) {
                    start_col = i;
                    found_start = true;
                    break;
                  }
                  depth--;
                }
              }

              if (!found_start) {
                for (size_t r = row; r > 0; r--) {
                  Line *prev_line = &buf->lines[r - 1];
                  for (size_t i = prev_line->length; i > 0; i--) {
                    if (prev_line->data[i - 1] == close_char) {
                      depth++;
                    } else if (prev_line->data[i - 1] == text_obj) {
                      if (depth == 0) {
                        start_row = r - 1;
                        start_col = i;
                        found_start = true;
                        break;
                      }
                      depth--;
                    }
                  }
                  if (found_start)
                    break;
                }
              }

              if (found_start) {
                depth = 0;
                bool search_current = (start_row == row);
                size_t search_start = search_current ? start_col : 0;

                for (size_t i = search_start; i < line->length; i++) {
                  if (line->data[i] == text_obj) {
                    depth++;
                  } else if (line->data[i] == close_char) {
                    if (depth == 0) {
                      end_row = row;
                      end_col = i;
                      found_end = true;
                      break;
                    }
                    depth--;
                  }
                }

                if (!found_end) {
                  for (size_t r = row + 1; r < buf->length; r++) {
                    Line *next_line = &buf->lines[r];
                    for (size_t i = 0; i < next_line->length; i++) {
                      if (next_line->data[i] == text_obj) {
                        depth++;
                      } else if (next_line->data[i] == close_char) {
                        if (depth == 0) {
                          end_row = r;
                          end_col = i;
                          found_end = true;
                          break;
                        }
                        depth--;
                      }
                    }
                    if (found_end)
                      break;
                  }
                }
              }
            } else {
              for (size_t i = col; i > 0; i--) {
                if (line->data[i - 1] == text_obj) {
                  start_col = i;
                  found_start = true;
                  break;
                }
              }

              if (!found_start) {
                for (size_t r = row; r > 0; r--) {
                  Line *prev_line = &buf->lines[r - 1];
                  for (size_t i = prev_line->length; i > 0; i--) {
                    if (prev_line->data[i - 1] == text_obj) {
                      start_row = r - 1;
                      start_col = i;
                      found_start = true;
                      break;
                    }
                  }
                  if (found_start)
                    break;
                }
              }

              if (found_start) {
                bool search_current = (start_row == row);
                size_t search_start = search_current ? start_col : 0;

                for (size_t i = search_start; i < line->length; i++) {
                  if (line->data[i] == close_char) {
                    end_row = row;
                    end_col = i;
                    found_end = true;
                    break;
                  }
                }

                if (!found_end) {
                  for (size_t r = row + 1; r < buf->length; r++) {
                    Line *next_line = &buf->lines[r];
                    for (size_t i = 0; i < next_line->length; i++) {
                      if (next_line->data[i] == close_char) {
                        end_row = r;
                        end_col = i;
                        found_end = true;
                        break;
                      }
                    }
                    if (found_end)
                      break;
                  }
                }
              }
            }

            if (found_start && found_end) {
              ctx->selection.start.row = start_row + 1;
              ctx->selection.start.column = start_col + 1;
              ctx->selection.end.row = end_row + 1;
              ctx->selection.end.column = end_col;
              update_selection_end = false;
            }
          }
        }
      }
    }
    break;
  }

  if (update_selection_end) {
    ctx->selection.end.row = window->cursor.row;
    ctx->selection.end.column = window->cursor.column;
  } else {
    Selection *sel = &ctx->selection;
    if (sel->end.row > sel->start.row ||
        (sel->end.row == sel->start.row && sel->end.column >= sel->start.column)) {
      window->cursor.row = sel->end.row;
      window->cursor.column = sel->end.column;
    } else {
      window->cursor.row = sel->start.row;
      window->cursor.column = sel->start.column;
    }
  }
}
