#include <stdlib.h>
#include <unistd.h>

#include "draw.h"
#include "edit.h"
#include "input.h"

#define MILLISECONDS 1000

void handle_input(Window *window, bool *running, EditorMode *mode, size_t width,
                  size_t height) {
  fd_set readfds;
  struct timeval tv;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 100 * MILLISECONDS;

  if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (*mode == MODE_NORMAL) {
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
        case 'i':
          *mode = MODE_INSERT;
          break;
        case 'q':
          *running = false;
          break;
        }
      } else if (*mode == MODE_INSERT) {
        switch (c) {
        case 27:
          *mode = MODE_NORMAL;
          break;
        case 127:
        case 8:
          delete_char(window);
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
      draw_screen(window, width, height);
    }
  }
}
