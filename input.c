#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "draw.h"
#include "input.h"
#include "main.h"
#include "mode_handlers.h"

#define MILLISECONDS 1000

void handle_input(Context *ctx) {
  Window *window = ctx->windows[ctx->current_window];
  size_t width = ctx->terminal.width;
  size_t height = ctx->terminal.height;

  fd_set readfds;
  struct timeval tv;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 100 * MILLISECONDS;

  if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (ctx->mode == MODE_NORMAL) {
        handle_normal_mode(ctx, c);
      } else if (ctx->mode == MODE_COMMAND) {
        handle_command_mode(ctx, c);
      } else if (ctx->mode == MODE_SEARCH) {
        handle_search_mode(ctx, c);
      } else if (ctx->mode == MODE_INSERT) {
        handle_insert_mode(ctx, c);
      } else if (ctx->mode == MODE_LINEWISE_VISUAL ||
                 ctx->mode == MODE_CHARACTERWISE_VISUAL) {
        handle_visual_mode(ctx, c);
      }
      draw_screen(window, width, height, ctx->mode, &ctx->selection,
                  ctx->command_buffer, ctx->command_buffer_length,
                  ctx->search_buffer, ctx->search_buffer_length,
                  ctx->show_line_numbers);
    }
  }
}
