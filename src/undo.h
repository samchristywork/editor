#ifndef UNDO_H
#define UNDO_H

#include "main.h"

void init_undo_stack(Context *ctx);

void free_undo_stack(Context *ctx);

void push_undo_state(Context *ctx);

void undo(Context *ctx);

#endif
