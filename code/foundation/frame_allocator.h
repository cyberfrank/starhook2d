#pragma once
#include "basic.h"

struct Allocator;

// Allocates memory for the current frame
void *frame_alloc(uint64_t size);

// Returns an allocator that uses `frame_alloc()`
struct Allocator *frame_allocator();

// Ticks a "frame" so that memory allocated by `frame_alloc()` can be freed
void frame_allocator_tick();

// Frame allocator printf
char *frame_vprintf(const char *format, va_list args);
char *frame_printf(const char *format, ...);

