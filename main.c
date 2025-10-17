#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "main.h"

#define MILLISECONDS 1000
Context *global_ctx;

void enter_alt_screen() {
  struct termios raw = global_ctx->terminal.attrs;
  raw.c_lflag &= ~(ECHO | ICANON);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  printf("\x1b[?1049h");
  printf("\x1b[?25l");
  printf("\x1b[2J");
  printf("\x1b[H");
  fflush(stdout);
}

void leave_alt_screen(Context ctx) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &ctx.terminal.attrs);
  printf("\x1b[?1049l");
  printf("\x1b[?25h");
  fflush(stdout);
}

int main(int argc, char *argv[]) {
}
