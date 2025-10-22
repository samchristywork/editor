#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include "main.h"
#include "text_objects.h"

bool find_text_object(Window *window, char text_obj, size_t *start_row,
                      size_t *start_col, size_t *end_row, size_t *end_col) {
  Buffer *buf = window->current_buffer;
  size_t row = window->cursor.row - 1;
  size_t col = window->cursor.column - 1;

  if (row >= buf->length)
    return false;

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

    *start_row = para_start;
    *start_col = 0;
    *end_row = para_end;
    *end_col = buf->lines[para_end].length;
    return true;
  } else if (text_obj == 'w') {
    if (col < line->length && isalnum((unsigned char)line->data[col])) {
      size_t start = col;
      size_t end = col;

      while (start > 0 && isalnum((unsigned char)line->data[start - 1])) {
        start--;
      }
      while (end < line->length && isalnum((unsigned char)line->data[end])) {
        end++;
      }

      *start_row = row;
      *start_col = start;
      *end_row = row;
      *end_col = end;
      return true;
    }
  } else if (text_obj == '"' || text_obj == '\'' || text_obj == '<' ||
             text_obj == '{' || text_obj == '[' || text_obj == '(') {
    char close_char;
    bool is_bracket = (text_obj == '<' || text_obj == '{' || text_obj == '[' ||
                       text_obj == '(');
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

    size_t s_row = row;
    size_t s_col = 0;
    size_t e_row = row;
    size_t e_col = 0;
    bool found_start = false;
    bool found_end = false;

    if (is_bracket) {
      int depth = 0;
      for (size_t i = col; i > 0; i--) {
        if (line->data[i - 1] == close_char) {
          depth++;
        } else if (line->data[i - 1] == text_obj) {
          if (depth == 0) {
            s_col = i;
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
                s_row = r - 1;
                s_col = i;
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
        bool search_current = (s_row == row);
        size_t search_start = search_current ? s_col : 0;

        for (size_t i = search_start; i < line->length; i++) {
          if (line->data[i] == text_obj) {
            depth++;
          } else if (line->data[i] == close_char) {
            if (depth == 0) {
              e_row = row;
              e_col = i;
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
                  e_row = r;
                  e_col = i;
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
          s_col = i;
          found_start = true;
          break;
        }
      }

      if (!found_start) {
        for (size_t r = row; r > 0; r--) {
          Line *prev_line = &buf->lines[r - 1];
          for (size_t i = prev_line->length; i > 0; i--) {
            if (prev_line->data[i - 1] == text_obj) {
              s_row = r - 1;
              s_col = i;
              found_start = true;
              break;
            }
          }
          if (found_start)
            break;
        }
      }

      if (found_start) {
        bool search_current = (s_row == row);
        size_t search_start = search_current ? s_col : 0;

        for (size_t i = search_start; i < line->length; i++) {
          if (line->data[i] == close_char) {
            e_row = row;
            e_col = i;
            found_end = true;
            break;
          }
        }

        if (!found_end) {
          for (size_t r = row + 1; r < buf->length; r++) {
            Line *next_line = &buf->lines[r];
            for (size_t i = 0; i < next_line->length; i++) {
              if (next_line->data[i] == close_char) {
                e_row = r;
                e_col = i;
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
      *start_row = s_row;
      *start_col = s_col;
      *end_row = e_row;
      *end_col = e_col;
      return true;
    }
  }

  return false;
}
