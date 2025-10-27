#include <stdio.h>

#include "main.h"
#include "save.h"

void save_buffer(Buffer *buffer) {
  if (buffer == NULL || buffer->file.name == NULL) {
    return;
  }

  FILE *f = fopen(buffer->file.name, "w");
  if (f == NULL) {
    return;
  }

  for (size_t i = 0; i < buffer->length; i++) {
    if (buffer->lines[i].length > 0) {
      fwrite(buffer->lines[i].data, 1, buffer->lines[i].length, f);
    }
    if (i < buffer->length) {
      fputc('\n', f);
    }
  }

  fclose(f);
}
