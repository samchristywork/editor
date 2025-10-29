#ifndef MODE_HANDLERS_H
#define MODE_HANDLERS_H

#include "main.h"

void handle_normal_mode(Context *ctx, unsigned char c);
void handle_command_mode(Context *ctx, unsigned char c);
void handle_search_mode(Context *ctx, unsigned char c);
void handle_filter_mode(Context *ctx, unsigned char c);
void handle_insert_mode(Context *ctx, unsigned char c);
void handle_visual_mode(Context *ctx, unsigned char c);

#endif
