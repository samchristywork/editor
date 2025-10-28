#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <termios.h>

typedef enum {
  MODE_COMMAND,
  MODE_INSERT,
  MODE_NORMAL,
  MODE_LINEWISE_VISUAL,
  MODE_CHARACTERWISE_VISUAL,
  MODE_SEARCH
} EditorMode;

typedef struct {
  size_t height;
  size_t width;
  struct termios attrs;
} Terminal;

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} Line;

typedef struct {
  char *name;
} File;

typedef struct {
  File file;
  Line *lines;
  size_t length;
} Buffer;

typedef struct {
  size_t row;
  size_t column;
} Position;

typedef struct {
  Position start;
  Position end;
} Selection;

typedef struct {
  size_t row;
  size_t column;
} Cursor;

typedef struct {
  size_t vertical;
  size_t horizontal;
} Scroll;

typedef struct {
  size_t row;
  size_t column;
  size_t height;
  size_t width;
  Cursor cursor;
  Scroll scroll;
  Buffer *current_buffer;
} Window;

typedef struct {
  Line *lines;
  size_t length;
  Cursor cursor;
} UndoState;

typedef struct {
  UndoState *states;
  size_t length;
  size_t capacity;
} UndoStack;

typedef struct {
  Terminal terminal;
  Window **windows;
  size_t n_windows;
  size_t current_window;
  Buffer **buffers;
  size_t n_buffers;
  bool running;
  EditorMode mode;
  Selection selection;
  char *command_buffer;
  size_t command_buffer_length;
  size_t command_buffer_capacity;
  char *search_buffer;
  size_t search_buffer_length;
  size_t search_buffer_capacity;
  char **yank_buffer;
  size_t *yank_buffer_lengths;
  size_t yank_buffer_length;
  bool yank_linewise;
  UndoStack undo_stack;
  bool show_line_numbers;
} Context;

typedef struct {
  bool in_keyword;
  bool in_block_comment;
  bool in_line_comment;
  bool in_single_quote_string;
  bool in_double_quote_string;
  bool in_preprocessor_directive;
} SyntaxState;

typedef struct {
  File *files;
  size_t length;
} FileList;

typedef struct {
  FileList file_list;
} Arguments;

#endif
