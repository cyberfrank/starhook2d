#pragma once
#include "basic.h"

struct Allocator;

struct Allocator *linear_allocator_create(uint64_t total_size, struct Allocator *backing);

void linear_allocator_destroy(struct Allocator *a);

void rewind_linear_allocator(struct Allocator *a);
