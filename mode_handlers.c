#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

  switch (c) {
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
    for (; leading_spaces < line_ptr->length; leading_spaces++) {
      if (line_ptr->data[leading_spaces] != ' ') {
        break;
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
    delete_char(window);
    break;
  case 'd':
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == 'd') {
        push_undo_state(ctx);
        delete_line(window);
      } else if (c == 'w') {
        push_undo_state(ctx);
        delete_word(window);
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
    for (window->cursor.column = 0;
         line_ptr->data[window->cursor.column] == ' ' &&
         window->cursor.column < line_ptr->length;
         window->cursor.column++) {
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
}

void handle_command_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;
  bool *running = &ctx->running;
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
    if (*command_buffer_length == 1 && (*command_buffer)[0] == 'w') {
      save_buffer(window->current_buffer);
    } else if (*command_buffer_length == 1 && (*command_buffer)[0] == 'q') {
      *running = false;
    } else if (*command_buffer_length == 1 && (*command_buffer)[0] == 'x') {
      save_buffer(window->current_buffer);
      *running = false;
    } else if (*command_buffer_length == 2 &&
               strncmp(*command_buffer, "bn", 2) == 0) {
      size_t current_idx = 0;
      for (size_t i = 0; i < ctx->n_buffers; i++) {
        if (ctx->buffers[i] == window->current_buffer) {
          current_idx = i;
          break;
        }
      }
      size_t next_idx = (current_idx + 1) % ctx->n_buffers;
      window->current_buffer = ctx->buffers[next_idx];
      window->cursor.row = 1;
      window->cursor.column = 1;
      window->scroll.vertical = 0;
      window->scroll.horizontal = 0;
    } else if (*command_buffer_length == 2 &&
               strncmp(*command_buffer, "bp", 2) == 0) {
      size_t current_idx = 0;
      for (size_t i = 0; i < ctx->n_buffers; i++) {
        if (ctx->buffers[i] == window->current_buffer) {
          current_idx = i;
          break;
        }
      }
      size_t prev_idx =
          (current_idx == 0) ? ctx->n_buffers - 1 : current_idx - 1;
      window->current_buffer = ctx->buffers[prev_idx];
      window->cursor.row = 1;
      window->cursor.column = 1;
      window->scroll.vertical = 0;
      window->scroll.horizontal = 0;
    } else if (*command_buffer_length > 0) {
      bool is_number = true;
      for (size_t i = 0; i < *command_buffer_length; i++) {
        if (!isdigit((*command_buffer)[i])) {
          is_number = false;
          break;
        }
      }
      if (is_number) {
        char *temp = realloc(*command_buffer, *command_buffer_length + 1);
        if (temp != NULL) {
          *command_buffer = temp;
          (*command_buffer)[*command_buffer_length] = '\0';
        }
        int line_number_int = atoi(*command_buffer);
        size_t line_number = line_number_int < 0 ? 0 : (size_t)line_number_int;
        if (line_number > window->current_buffer->length) {
          line_number = window->current_buffer->length;
        }
        window->cursor.row = line_number;
        window->cursor.column = 1;
      }
    }
    *mode = MODE_NORMAL;
    free(*command_buffer);
    *command_buffer = NULL;
    *command_buffer_length = 0;
    break;
  case 127:
  case 8:
    if (*command_buffer_length > 0) {
      (*command_buffer_length)--;
      if (*command_buffer_length > 0) {
        char *temp = realloc(*command_buffer, *command_buffer_length);
        if (temp != NULL) {
          *command_buffer = temp;
        }
      } else {
        free(*command_buffer);
        *command_buffer = NULL;
      }
    }
    break;
  default:
    if (c >= 32 && c <= 126) {
      char *temp = realloc(*command_buffer, *command_buffer_length + 1);
      if (temp != NULL) {
        *command_buffer = temp;
        (*command_buffer)[*command_buffer_length] = c;
        (*command_buffer_length)++;
      }
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
      if (*search_buffer_length > 0) {
        char *temp = realloc(*search_buffer, *search_buffer_length);
        if (temp != NULL) {
          *search_buffer = temp;
        }
      } else {
        free(*search_buffer);
        *search_buffer = NULL;
      }
    }
    break;
  default:
    if (c >= 32 && c <= 126) {
      char *temp = realloc(*search_buffer, *search_buffer_length + 1);
      if (temp != NULL) {
        *search_buffer = temp;
        (*search_buffer)[*search_buffer_length] = c;
        (*search_buffer_length)++;
      }
    }
    break;
  }
}

void handle_insert_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;

  switch (c) {
  case 27: {
    unsigned char next;
    if (read(STDIN_FILENO, &next, 1) == 1) {
      if (next == '[') {
        unsigned char seq;
        if (read(STDIN_FILENO, &seq, 1) == 1 && seq == 'Z') {
          insert_char(window, '\t');
          break;
        }
      }
    }
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

void handle_visual_mode(Context *ctx, unsigned char c) {
  Window *window = ctx->windows[ctx->current_window];
  EditorMode *mode = &ctx->mode;
  bool update_selection_end = true;

  switch (c) {
  case 27:
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
    *mode = MODE_NORMAL;
    break;
  case 'd':
    push_undo_state(ctx);
    delete_selection(ctx);
    *mode = MODE_NORMAL;
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
  }
}
