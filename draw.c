#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "main.h"

#define COLOR_KEYWORD "\x1b[35m"
#define COLOR_STRING "\x1b[33m"
#define COLOR_COMMENT "\x1b[90m"
#define COLOR_PREPROCESSOR "\x1b[36m"
#define COLOR_NUMBER "\x1b[31m"
#define COLOR_TAB "\x1b[34m"
#define COLOR_RESET "\x1b[0m"

static const char *keywords[] = {
    "auto",     "break",  "case",    "char",   "const",    "continue",
    "default",  "do",     "double",  "else",   "enum",     "extern",
    "float",    "for",    "goto",    "if",     "int",      "long",
    "register", "return", "short",   "signed", "sizeof",   "static",
    "struct",   "switch", "typedef", "union",  "unsigned", "void",
    "volatile", "while",  "bool",    "true",   "false",    NULL};

static bool is_keyword(const char *word, size_t len) {
  for (size_t i = 0; keywords[i] != NULL; i++) {
    if (strlen(keywords[i]) == len && strncmp(word, keywords[i], len) == 0) {
      return true;
    }
  }
  return false;
}

static bool update_syntax_state(SyntaxState *state, char c, char next_c,
                                const char *line_start, const char *current_pos,
                                bool *is_closing_char) {
  *is_closing_char = false;

  if (state->in_block_comment) {
    if (c == '*' && next_c == '/') {
      *is_closing_char = true;
      return true;
    }
    return true;
  }

  if (state->in_line_comment) {
    return true;
  }

  if (state->in_single_quote_string) {
    if (c == '\'' &&
        (current_pos == line_start || *(current_pos - 1) != '\\')) {
      *is_closing_char = true;
      return true;
    }
    return true;
  }

  if (state->in_double_quote_string) {
    if (c == '"' && (current_pos == line_start || *(current_pos - 1) != '\\')) {
      *is_closing_char = true;
      return true;
    }
    return true;
  }

  if (c == '/' && next_c == '*') {
    state->in_block_comment = true;
    return true;
  }

  if (c == '/' && next_c == '/') {
    state->in_line_comment = true;
    return true;
  }

  if (c == '\'') {
    state->in_single_quote_string = true;
    return true;
  }

  if (c == '"') {
    state->in_double_quote_string = true;
    return true;
  }

  if (c == '#' && current_pos == line_start) {
    state->in_preprocessor_directive = true;
    return true;
  }

  return false;
}

static const char *get_syntax_color(SyntaxState *state, bool is_num) {
  if (state->in_block_comment || state->in_line_comment) {
    return COLOR_COMMENT;
  }
  if (state->in_single_quote_string || state->in_double_quote_string) {
    return COLOR_STRING;
  }
  if (state->in_preprocessor_directive) {
    return COLOR_PREPROCESSOR;
  }
  if (state->in_keyword) {
    return COLOR_KEYWORD;
  }
  if (is_num) {
    return COLOR_NUMBER;
  }
  return NULL;
}

static void set_cursor_position(size_t row, size_t column) {
  printf("\x1b[%zu;%zuH", row, column);
}

static void draw_status_bar(size_t width, size_t height, Cursor cursor,
                            EditorMode mode, char *command_buffer,
                            size_t command_buffer_length, char *search_buffer,
                            size_t search_buffer_length, const char *filename) {
  set_cursor_position(height, 1);
  printf("\x1b[7m");
  char status_bar_text[256];
  if (mode == MODE_COMMAND) {
    snprintf(status_bar_text, 256, ":%.*s", (int)command_buffer_length,
             command_buffer ? command_buffer : "");
  } else if (mode == MODE_SEARCH) {
    snprintf(status_bar_text, 256, "/%.*s", (int)search_buffer_length,
             search_buffer ? search_buffer : "");
  } else if (mode == MODE_LINEWISE_VISUAL) {
    snprintf(status_bar_text, 256, "%s -- VISUAL LINE -- %zu %zu",
             filename ? filename : "[No Name]", cursor.row, cursor.column);
  } else if (mode == MODE_CHARACTERWISE_VISUAL) {
    snprintf(status_bar_text, 256, "%s -- VISUAL -- %zu %zu",
             filename ? filename : "[No Name]", cursor.row, cursor.column);
  } else {
    snprintf(status_bar_text, 256, "%s %zu %zu",
             filename ? filename : "[No Name]", cursor.row, cursor.column);
  }
  size_t len = strlen(status_bar_text);
  for (size_t i = 0; i < width; i++) {
    if (i < len) {
      putchar(status_bar_text[i]);
    } else {
      putchar(' ');
    }
  }
  printf("\x1b[0m");
  fflush(stdout);
}

static bool is_in_selection(size_t row, size_t col, EditorMode mode,
                            Selection *selection) {
  if (mode != MODE_LINEWISE_VISUAL && mode != MODE_CHARACTERWISE_VISUAL) {
    return false;
  }

  size_t start_row = selection->start.row;
  size_t start_col = selection->start.column;
  size_t end_row = selection->end.row;
  size_t end_col = selection->end.column;

  if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
    size_t tmp = start_row;
    start_row = end_row;
    end_row = tmp;
    tmp = start_col;
    start_col = end_col;
    end_col = tmp;
  }

  if (mode == MODE_LINEWISE_VISUAL) {
    return row >= start_row && row <= end_row;
  } else {
    if (row < start_row || row > end_row) {
      return false;
    }
    if (start_row == end_row) {
      return col >= start_col && col <= end_col;
    } else if (row == start_row) {
      return col >= start_col;
    } else if (row == end_row) {
      return col <= end_col;
    } else {
      return true;
    }
  }
}

static void compute_syntax_state_up_to_line(Buffer *buffer, size_t target_line,
                                             SyntaxState *state) {
  *state = (SyntaxState){0};

  for (size_t row = 0; row < target_line && row < buffer->length; row++) {
    Line *line = &buffer->lines[row];

    for (size_t col = 0; col < line->length; col++) {
      char c = line->data[col];
      char next_c = (col + 1 < line->length) ? line->data[col + 1] : '\0';
      bool is_closing_char = false;

      update_syntax_state(state, c, next_c, line->data, line->data + col,
                          &is_closing_char);

      if (is_closing_char) {
        if (c == '*') {
          state->in_block_comment = false;
        } else if (c == '\'') {
          state->in_single_quote_string = false;
        } else if (c == '"') {
          state->in_double_quote_string = false;
        }
      }
    }

    state->in_line_comment = false;
    state->in_preprocessor_directive = false;
    state->in_keyword = false;
  }
}

static void draw_line(Window *window, Line line, size_t n, EditorMode mode,
                      Selection *selection, SyntaxState *syntax_state) {
  Cursor cursor = window->cursor;
  Scroll scroll = window->scroll;
  set_cursor_position(window->row + n, window->column);

  bool *is_keyword_char = calloc(line.length, sizeof(bool));
  bool *is_number_char = calloc(line.length, sizeof(bool));

  if (is_keyword_char && is_number_char) {
    size_t word_start = 0;
    size_t word_len = 0;
    bool in_word = false;

    for (size_t col = 0; col < line.length; col++) {
      char c = line.data[col];
      if (isalnum(c) || c == '_') {
        if (!in_word) {
          in_word = true;
          word_start = col;
          word_len = 0;
        }
        word_len++;
      } else {
        if (in_word && word_len > 0) {
          if (is_keyword(line.data + word_start, word_len)) {
            for (size_t k = word_start; k < word_start + word_len; k++) {
              is_keyword_char[k] = true;
            }
          } else if (isdigit(line.data[word_start])) {
            for (size_t k = word_start; k < word_start + word_len; k++) {
              is_number_char[k] = true;
            }
          }
        }
        in_word = false;
        word_len = 0;
      }
    }
    if (in_word && word_len > 0) {
      if (is_keyword(line.data + word_start, word_len)) {
        for (size_t k = word_start; k < word_start + word_len; k++) {
          is_keyword_char[k] = true;
        }
      } else if (isdigit(line.data[word_start])) {
        for (size_t k = word_start; k < word_start + word_len; k++) {
          is_number_char[k] = true;
        }
      }
    }
  }

  // First, process the entire line to update syntax state correctly
  for (size_t col = 0; col < line.length; col++) {
    char c = line.data[col];
    char next_c = (col + 1 < line.length) ? line.data[col + 1] : '\0';
    bool is_closing_char = false;

    update_syntax_state(syntax_state, c, next_c, line.data,
                        line.data + col, &is_closing_char);

    if (is_closing_char) {
      if (c == '*') {
        syntax_state->in_block_comment = false;
      } else if (c == '\'') {
        syntax_state->in_single_quote_string = false;
      } else if (c == '"') {
        syntax_state->in_double_quote_string = false;
      }
    }
  }

  // Reset state to beginning of line for rendering
  SyntaxState line_render_state;
  compute_syntax_state_up_to_line(window->current_buffer, scroll.vertical + n,
                                   &line_render_state);

  // Process characters from start of line up to first visible column
  for (size_t col = 0; col < scroll.horizontal && col < line.length; col++) {
    char c = line.data[col];
    char next_c = (col + 1 < line.length) ? line.data[col + 1] : '\0';
    bool is_closing_char = false;

    update_syntax_state(&line_render_state, c, next_c, line.data,
                        line.data + col, &is_closing_char);

    if (is_closing_char) {
      if (c == '*') {
        line_render_state.in_block_comment = false;
      } else if (c == '\'') {
        line_render_state.in_single_quote_string = false;
      } else if (c == '"') {
        line_render_state.in_double_quote_string = false;
      }
    }
  }

  bool prev_was_block_comment_star = false;
  for (size_t i = 0; i < window->width; i++) {
    size_t buffer_col = scroll.horizontal + i;
    size_t buffer_row = scroll.vertical + n;
    bool on_cursor =
        cursor.row - 1 == buffer_row && cursor.column - 1 == buffer_col;
    bool in_selection =
        is_in_selection(buffer_row + 1, buffer_col + 1, mode, selection);

    char c = (buffer_col < line.length) ? line.data[buffer_col] : ' ';
    char next_c =
        (buffer_col + 1 < line.length) ? line.data[buffer_col + 1] : '\0';

    bool is_closing_char = false;
    bool is_num = false;

    if (buffer_col < line.length) {
      update_syntax_state(&line_render_state, c, next_c, line.data,
                          line.data + buffer_col, &is_closing_char);

      if (is_keyword_char && buffer_col < line.length) {
        line_render_state.in_keyword = is_keyword_char[buffer_col];
      }
      if (is_number_char && buffer_col < line.length) {
        is_num = is_number_char[buffer_col];
      }
    }

    bool needs_reset = false;
    bool is_tab = (c == '\t');
    bool in_comment_for_display = line_render_state.in_block_comment ||
                                   line_render_state.in_line_comment ||
                                   (prev_was_block_comment_star && c == '/');

    if (on_cursor) {
      printf("\x1b[7m");
      needs_reset = true;
    } else if (in_selection) {
      printf("\x1b[48;5;240m");
      needs_reset = true;
    } else if (is_tab) {
      printf(COLOR_TAB);
      needs_reset = true;
    } else if (in_comment_for_display) {
      printf(COLOR_COMMENT);
      needs_reset = true;
    } else {
      const char *color = get_syntax_color(&line_render_state, is_num);
      if (color) {
        printf("%s", color);
        needs_reset = true;
      }
    }

    if (is_tab) {
      printf(">>");
    } else {
      putchar(c);
    }

    if (needs_reset) {
      printf(COLOR_RESET);
    }

    if (is_closing_char) {
      if (c == '*') {
        prev_was_block_comment_star = true;
        line_render_state.in_block_comment = false;
      } else if (c == '\'') {
        line_render_state.in_single_quote_string = false;
      } else if (c == '"') {
        line_render_state.in_double_quote_string = false;
      }
    } else {
      prev_was_block_comment_star = false;
    }
  }

  // End of line - reset line-specific states
  syntax_state->in_line_comment = false;
  syntax_state->in_preprocessor_directive = false;
  syntax_state->in_keyword = false;

  if (is_keyword_char) {
    free(is_keyword_char);
  }
  if (is_number_char) {
    free(is_number_char);
  }
}

static void constrain_cursor(Window *window) {
  if (window->cursor.row < 1)
    window->cursor.row = 1;
  if (window->cursor.column < 1)
    window->cursor.column = 1;
  if (window->cursor.row > window->current_buffer->length)
    window->cursor.row = window->current_buffer->length;
  if (window->cursor.column >
      window->current_buffer->lines[window->cursor.row - 1].length + 1)
    window->cursor.column =
        window->current_buffer->lines[window->cursor.row - 1].length + 1;
}

static void update_scroll(Window *window) {
  if (window->cursor.row < window->scroll.vertical + 1) {
    window->scroll.vertical = window->cursor.row - 1;
  }
  if (window->cursor.row > window->scroll.vertical + window->height) {
    window->scroll.vertical = window->cursor.row - window->height;
  }
  if (window->cursor.column < window->scroll.horizontal + 1) {
    window->scroll.horizontal = window->cursor.column - 1;
  }
  if (window->cursor.column > window->scroll.horizontal + window->width) {
    window->scroll.horizontal = window->cursor.column - window->width;
  }
}

static void draw_window(Window *window, EditorMode mode, Selection *selection) {
  Buffer *current_buffer = window->current_buffer;
  SyntaxState syntax_state;
  compute_syntax_state_up_to_line(current_buffer, window->scroll.vertical,
                                   &syntax_state);

  Line eof_line = {.data = "~", .length = 1};
  for (size_t i = 0; i < window->height; i++) {
    size_t buffer_row = window->scroll.vertical + i;
    if (buffer_row < current_buffer->length) {
      draw_line(window, current_buffer->lines[buffer_row], i, mode, selection, &syntax_state);
    } else {
      draw_line(window, eof_line, i, mode, selection, &syntax_state);
    }
  }
  fflush(stdout);
}

static void draw_window_with_line_numbers(Window *window, EditorMode mode,
                                          Selection *selection,
                                          bool show_line_numbers) {
  if (!show_line_numbers) {
    draw_window(window, mode, selection);
    return;
  }

  Buffer *current_buffer = window->current_buffer;
  SyntaxState syntax_state;
  compute_syntax_state_up_to_line(current_buffer, window->scroll.vertical,
                                   &syntax_state);
  size_t num_digits = snprintf(NULL, 0, "%zu", current_buffer->length);
  if (num_digits < 3)
    num_digits = 3;

  size_t line_num_width = num_digits + 1;
  size_t saved_column = window->column;
  window->column += line_num_width;

  Line eof_line = {.data = "~", .length = 1};
  for (size_t i = 0; i < window->height; i++) {
    size_t buffer_row = window->scroll.vertical + i;

    printf("\033[%zu;1H", window->row + i);

    if (buffer_row < current_buffer->length) {
      printf("\033[38;5;242m%*zu \033[0m", (int)num_digits, buffer_row + 1);
      draw_line(window, current_buffer->lines[buffer_row], i, mode, selection, &syntax_state);
    } else {
      printf("\033[38;5;242m%*s \033[0m", (int)num_digits, "~");
      draw_line(window, eof_line, i, mode, selection, &syntax_state);
    }
  }

  window->column = saved_column;
  fflush(stdout);
}

void draw_screen(Window *window, size_t width, size_t height, EditorMode mode,
                 Selection *selection, char *command_buffer,
                 size_t command_buffer_length, char *search_buffer,
                 size_t search_buffer_length, bool show_line_numbers) {
  constrain_cursor(window);
  window->row = 1;
  window->column = 1;
  window->width = width;
  window->height = height - 1;
  update_scroll(window);
  draw_window_with_line_numbers(window, mode, selection, show_line_numbers);
  draw_status_bar(width, height, window->cursor, mode, command_buffer,
                  command_buffer_length, search_buffer, search_buffer_length,
                  window->current_buffer->file.name);
}
