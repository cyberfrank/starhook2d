#pragma once
#include "foundation/basic.h"

typedef struct Window_Settings {
    int width;
    int height;
    float scale;
    const char *title;
    Vec4 titlebar_color;
    bool vsync;
    bool hide_cursor;
} Window_Settings;

void *window_setup(Window_Settings *settings);
void window_destroy(void *window);

bool is_window_close_requested(void *window);

Rect window_rect(void *window);

void poll_events();

void window_swap_buffers(void *window);