#ifndef TEXT_OBJECTS_H
#define TEXT_OBJECTS_H

#include <stdbool.h>
#include <stddef.h>

#include "main.h"

bool find_text_object(Window *window, char text_obj, size_t *start_row,
                      size_t *start_col, size_t *end_row, size_t *end_col);

#endif
