#include "atom.h"
#include "allocator.h"
#include "hash.h"
#include "murmur_hash64.h"
#include "log.h"

struct Atom_Table {
    Allocator *allocator;
    uint8_t *data;
    uint64_t offset;
    uint64_t capacity;
    Hash lookup;
};

Atom_Table *atom_table_create(uint64_t capacity, Allocator *a)
{
    Atom_Table *table = c_alloc(a, sizeof(*table));
    void *data = c_alloc(a, capacity);
    memset(data, 0, capacity);
    *table = (Atom_Table) {
        .allocator = a,
        .data = data,
        .offset = 0,
        .capacity = capacity,
        .lookup = (Hash) { 0 },
    };
    return table;
}

void atom_table_destroy(Atom_Table *table)
{
    hash_free(&table->lookup, table->allocator);
    c_free(table->allocator, table->data, table->capacity);
    c_free(table->allocator, table, sizeof(*table));
}

Atom *atom_add(Atom_Table *table, String8 str)
{
    uint64_t key = murmur_hash64a(str.str, str.len, 0);
    Atom *res = (Atom *)hash_get(&table->lookup, key);
    if (res != 0) 
    {
        ++res->occurences;
        return res;
    }

    uint64_t needed_size = sizeof(Atom) + str.len + 1;
    fatal_checkf((table->offset + needed_size) < table->capacity, "Atom table out of memory");

    Atom *atom = (Atom *)(table->data + table->offset);
    memset(atom, 0, needed_size);
    table->offset += needed_size;

    atom->hash = key;
    atom->len = str.len;
    atom->str = (uint8_t *)atom + sizeof(Atom);
    memcpy((uint8_t *)atom->str, str.str, str.len);

    hash_add(&table->lookup, key, (uint64_t)atom, table->allocator);

    return atom;
}

void atom_table_rewind(Atom_Table *table)
{
    table->offset = 0;
    memset(table->data, 0, table->capacity);
    hash_clear(&table->lookup);
}

Atom *atom_table_data(Atom_Table *table)
{
    return (Atom *)table->data;
}