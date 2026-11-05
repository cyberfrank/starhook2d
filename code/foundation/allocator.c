#include "allocator.h"
#include "os.h"
#include "log.h"
#include <stdlib.h>

static void *system_alloc(Allocator *allocator, void *old_ptr, uint64_t old_size, uint64_t new_size, 
        const char *file, uint32_t line)
{
    void *new_ptr = 0;
    if (new_size != 0)
    {
        new_ptr = realloc(old_ptr, new_size);
    }
    else
    {
        free(old_ptr);
    }

    record_allocation(old_ptr, old_size, new_ptr, new_size, file, line);

    return new_ptr;
}

static void *fixed_vm_alloc(Allocator *allocator, void *old_ptr, uint64_t old_size, uint64_t new_size, 
        const char *file, uint32_t line)
{
    old_size = ALIGN_SIZE(old_size, PAGE_SIZE);
    new_size = ALIGN_SIZE(new_size, PAGE_SIZE);

    if (new_size > 0 && new_size <= old_size)
    {
        return old_ptr;
    }

    uint64_t reserve_size = (uint64_t)allocator->user_data;
    fatal_checkf(new_size <= reserve_size, "Fixed virtual memory allocator out of memory!");

    void *new_ptr = 0;
    if (old_ptr == 0 && new_size > 0) 
    {
        new_ptr = os_reserve(reserve_size);
        os_commit(new_ptr, new_size);
    } 
    else if (new_size > 0) 
    {
        new_ptr = old_ptr;
        os_commit((char *)new_ptr + old_size, new_size - old_size);
    } 
    else if (new_size == 0) 
    {
        new_ptr = 0;
        os_release(old_ptr);
    }

    record_allocation(old_ptr, old_size, new_ptr, new_size, file, line);

    return new_ptr;
}

static int64_t bytes_allocated = 0;

void record_allocation(void *old_ptr, uint64_t old_size, 
    void *new_ptr, uint64_t new_size, const char *file, uint32_t line)
{
    bytes_allocated += (new_size - old_size);
}

int64_t total_bytes_allocated()
{
    return bytes_allocated;
}

Allocator allocator_create_fixed_vm(uint64_t reserve_size)
{
    Allocator a = {
        .allocate_func = fixed_vm_alloc,
        .user_data = (void *)reserve_size,
    };
    return a;
}

Allocator *system_allocator = &(Allocator)
{
    .allocate_func = system_alloc,
    .user_data = 0,
};