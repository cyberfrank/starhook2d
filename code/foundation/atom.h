#pragma once
#include "basic.h"

struct Allocator;
typedef struct Atom_Table Atom_Table;
typedef struct Atom Atom;

struct Atom {
    const char *str;
    uint64_t len;
    uint64_t hash;
    uint32_t occurences;
};

Atom_Table *atom_table_create(uint64_t capacity, struct Allocator *a);
void atom_table_destroy(Atom_Table *table);

Atom *atom_add(Atom_Table *table, String8 str);

void atom_table_rewind(Atom_Table *table);

Atom *atom_table_data(Atom_Table *table);

static inline Atom *atom_next(Atom *a)
{
    return (Atom *)((uint8_t *)a + (a->len + sizeof(*a) + 1));
}