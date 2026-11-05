#include "linear_allocator.h"
#include "allocator.h"
#include "log.h"

typedef struct Linear_Allocator_Data {
    uint8_t *p;
    uint64_t offset;
    uint64_t total_size;
    Allocator *backing;
} Linear_Allocator_Data;

static void *linear_alloc(Allocator *allocator, void *old_ptr, uint64_t old_size, uint64_t new_size, 
        const char *file, uint32_t line)
{
    Linear_Allocator_Data *data = allocator->user_data;

    void *result = 0;
    if (new_size != 0)
    {
        fatal_checkf("Linear allocator exhausted!\
            Attempted to allocated %zu bytes which would exceed the maximum of %zu bytes",
            new_size, data->total_size);
        
        result = data->p + data->offset;
        data->offset += new_size;
    }
    return result;
}

Allocator *linear_allocator_create(uint64_t total_size, Allocator *backing)
{
    check(total_size != 0);

    Allocator *a = c_alloc(backing, sizeof(*a));
    *a = (Allocator) {
        .allocate_func = linear_alloc,
        .user_data = c_alloc(backing, sizeof(Linear_Allocator_Data)),
    };

    Linear_Allocator_Data *data = a->user_data;
    data->p = c_alloc(backing, total_size);
    data->offset = 0;
    data->total_size = total_size;
    data->backing = backing;

    return a;
}

void linear_allocator_destroy(Allocator *a)
{
    Linear_Allocator_Data *data = a->user_data;
    check(data && data->total_size != 0);

    Allocator *backing = data->backing;
    c_free(backing, data->p, data->total_size);
    c_free(backing, data, sizeof(*data));
    c_free(backing, a, sizeof(*a));
}

void rewind_linear_allocator(struct Allocator *a)
{
    Linear_Allocator_Data *data = a->user_data;
    check(data && data->total_size != 0);
    data->offset = 0;
}