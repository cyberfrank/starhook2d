#include "frame_allocator.h"
#include "allocator.h"
#include "sprintf.h"
#include <string.h>
#include <stdarg.h>

typedef struct Frame_Allocator_Block
{
    uint64_t size;
    uint64_t last_used;
    uint64_t used;
    struct Frame_Allocator_Block *next;
} Frame_Allocator_Block;

Allocator frame_allocator_backing;
Frame_Allocator_Block empty = { 0 };
Frame_Allocator_Block *frame_block = &empty;
Frame_Allocator_Block *last_frame_block = &empty;

static uint64_t frame_allocation_blocks = 0;

static Frame_Allocator_Block *fa__allocate_block(Allocator *backing, uint64_t size)
{
    Frame_Allocator_Block *block = c_alloc(backing, size);
    *block = (Frame_Allocator_Block) {
        .size = size,
        .last_used = sizeof(*block),
        .used = sizeof(*block),
    };
    return block;
}

static void *fa__block_realloc(Frame_Allocator_Block **first, void *ptr, 
    uint64_t old_size, uint64_t new_size, Allocator *backing)
{
    Frame_Allocator_Block *block = *first;

    // Free if this is a reallocation of the last allocation
    if (block && (char *)ptr == (char *)block + block->last_used)
    {
        block->used = block->last_used;
    }

    // Freeing
    if (new_size == 0)
    {
        return 0;
    }

    // Check if allocation fits in block
    if (block && block->size - block->used >= new_size)
    {
        void *res = (char *)block + block->used;
        block->last_used = block->used;
        block->used += new_size;
        if (res != ptr && old_size != 0 && new_size != 0)
        {
            memcpy(res, ptr, old_size < new_size ? old_size : new_size);
        }
        return res;
    }

    uint64_t new_block_size = (block && block->size && block->size * 2 >= PAGE_SIZE) ? (block->size * 2) : PAGE_SIZE;
    while ((new_block_size - sizeof(Frame_Allocator_Block)) < new_size)
    {
        new_block_size *= 2;
    }

    Frame_Allocator_Block *new_block = fa__allocate_block(backing, new_block_size);
    new_block->next = block;
    *first = new_block;

    void *res = fa__block_realloc(first, ptr, old_size, new_size, backing);
    if (res != ptr && old_size != 0)
    {
        memcpy(res, ptr, old_size);
    }

    return res;
}

static void *fa__frame_alloc(Allocator *allocator, void *old_ptr, uint64_t old_size, uint64_t new_size, 
        const char *file, uint32_t line)
{
    if (old_size != 0 && new_size != 0 && new_size <= old_size)
    {
        return old_ptr;
    }
    void *new_ptr = 0;
    if (new_size != 0)
    {
        new_ptr = frame_alloc(new_size);
    }
    if (old_size != 0 && new_size != 0)
    {
        memcpy(new_ptr, old_ptr, old_size);
    }
    return new_ptr;
}

void *frame_alloc(uint64_t size)
{
    if (!frame_allocator_backing.allocate_func)
    {
        frame_allocator_backing = *system_allocator;
    }
    size = ALIGN_SIZE(size, 8);
    void *p = fa__block_realloc(&frame_block, (void *)0, 0, size, &frame_allocator_backing);
    return p;
}

struct Allocator frame_allocator_object = {
    .allocate_func = fa__frame_alloc,
    .user_data = 0,
};

struct Allocator *frame_allocator()
{
    return &frame_allocator_object;
}

void frame_allocator_tick()
{   
    if (frame_allocator_backing.allocate_func)
    {
        while (last_frame_block != &empty) 
        {
            Frame_Allocator_Block *next = last_frame_block->next;
            c_free(&frame_allocator_backing, last_frame_block, last_frame_block->size);
            last_frame_block = next;
        }
        last_frame_block = frame_block;
        frame_block = &empty;
    }
}

char *frame_vprintf(const char *format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int32_t n = c_vprint(NULL, 0, format, args_copy);
    va_end(args_copy);

    char *buffer = frame_alloc((uint64_t)n + 1);
    c_vprint(buffer, n + 1, format, args);
    return buffer;
}

char *frame_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char *buf = frame_vprintf(format, args);

    va_end(args);
    return buf;
}