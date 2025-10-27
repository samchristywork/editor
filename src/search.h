#ifndef SEARCH_H
#define SEARCH_H

#include <stdbool.h>
#include <stddef.h>

#include "main.h"

typedef enum { SEARCH_FORWARD, SEARCH_BACKWARD } SearchDirection;

bool find_occurrence(Window *window, const char *search_str, size_t search_len,
                     SearchDirection direction);

#endif
