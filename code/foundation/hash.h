#pragma once
#include "basic.h"
#include "allocator.h"

#include <string.h>

static const uint64_t HASH_TOMBSTONE = 0xfffffffffffffffeULL;
static const uint64_t HASH_UNUSED = 0xffffffffffffffffULL;

typedef struct Hash
{
    uint32_t num_buckets;
    uint64_t *keys;
    uint64_t *values;
} Hash;

// Check if the hash contains `key`
static inline bool hash_has(const Hash *hash, uint64_t key);

// Get the value at `key` if it exists, otherwise 0
static inline uint64_t hash_get(const Hash *hash, uint64_t key);

// Get the value at `key` if it exists, otherwise the default value `def`
static inline uint64_t hash_get_default(const Hash *hash, uint64_t key, uint64_t def);

// Update the value stored at `key`
static inline void hash_update(Hash *hash, uint64_t key, uint64_t value);

// Add or probe a new slot for `key` 
static inline uint64_t *hash_add_reference(Hash *hash, uint64_t key, Allocator *a);

// Shorthand for *hash_add_reference(hash, key) = value;
static inline void hash_add(Hash *hash, uint64_t key, uint64_t value, Allocator *a);

// Remove the value stored at `key`
static inline uint64_t hash_remove(Hash *hash, uint64_t key);

// Clear the hash
static inline void hash_clear(Hash *hash);

// Release all memory of `hash`
static inline void hash_free(Hash *hash, Allocator *a);

static inline uint32_t hash__first_index(const Hash *hash, uint64_t key)
{
    return key % hash->num_buckets;
}

static inline uint32_t hash__next_index(const Hash *hash, uint32_t i)
{
    return (i + 1) % hash->num_buckets;
}

static inline uint32_t hash__index(const Hash *hash, uint64_t key)
{
    if (!hash->num_buckets || key >= HASH_TOMBSTONE)
        return UINT32_MAX;

    const uint32_t max_distance = 6;

    uint32_t i = hash__first_index(hash, key);
    uint32_t distance = 0;
    while (hash->keys[i] != key)
    {
        if (distance > max_distance)
            return UINT32_MAX;
        if (hash->keys[i] == HASH_UNUSED)
            return UINT32_MAX;
        i = hash__next_index(hash, i);
        ++distance;
    }
    return i;
}

static inline uint32_t hash__add_index(Hash *hash, uint64_t key)
{
    const uint32_t max_distance = 6;

    if (!hash->num_buckets)
        return UINT32_MAX;

    uint32_t i = hash__first_index(hash, key);
    uint32_t distance = 0;
    while (hash->keys[i] != HASH_UNUSED && hash->keys[i] != HASH_TOMBSTONE)
    {
        if (distance > max_distance)
            return UINT32_MAX;
        i = hash__next_index(hash, i);
        ++distance;
    }
    return i;
}

static inline void hash__grow(Hash *hash, Allocator *a)
{
    uint32_t new_buckets = (hash->num_buckets * 2) + 11;
    Hash new_hash = *hash;
    new_hash.num_buckets = new_buckets;
    new_hash.keys = c_alloc(a, new_buckets * (sizeof(*hash->keys) + sizeof(*hash->values)));
    new_hash.values = new_hash.keys + new_hash.num_buckets;
    hash_clear(&new_hash);
    for (uint32_t i = 0; i < hash->num_buckets; ++i)
    {
        if (hash->keys[i] != HASH_UNUSED && hash->keys[i] != HASH_TOMBSTONE)
            hash_add(&new_hash, hash->keys[i], hash->values[i], a);
    }	
    hash_free(hash, a);
    *hash = new_hash;
}

static inline bool hash_has(const Hash *hash, uint64_t key)
{
    return hash__index(hash, key) != UINT32_MAX;
}

static inline uint64_t hash_get(const Hash *hash, uint64_t key)
{
    const uint32_t i = hash__index(hash, key);
    return i != UINT32_MAX ? hash->values[i] : 0;
}

static inline uint64_t hash_get_default(const Hash *hash, uint64_t key, uint64_t def)
{
    const uint32_t i = hash__index(hash, key);
    return i != UINT32_MAX ? hash->values[i] : def;
}

static inline void hash_update(Hash *hash, uint64_t key, uint64_t value)
{
    uint32_t i = hash__index(hash, key);
    if (i != UINT32_MAX)
        hash->values[i] = value;
}

static inline uint64_t *hash_add_reference(Hash *hash, uint64_t key, Allocator *a)
{
    uint32_t i = hash__index(hash, key);
    if (i != UINT32_MAX)
        return hash->values + i;

    while (true)
    {
        i = hash__add_index(hash, key);
        if (i != UINT32_MAX)
            break;
        hash__grow(hash, a);
    }

    hash->keys[i] = key;
    hash->values[i] = 0;
    return hash->values + i;
}

static inline void hash_add(Hash *hash, uint64_t key, uint64_t value, Allocator *a) 
{
    *hash_add_reference(hash, key, a) = value;
}

static inline uint64_t hash_remove(Hash *hash, uint64_t key)
{
    uint32_t i = hash__index(hash, key);
    if (i != UINT32_MAX)
    {
        hash->keys[i] = HASH_TOMBSTONE;
        return hash->values[i];
    }
    return 0;
}

static inline void hash_clear(Hash *hash)
{
    memset(hash->keys, 0xff, hash->num_buckets * (sizeof(*hash->keys) + sizeof(*hash->values)));
}

static inline void hash_free(Hash *hash, Allocator *a)
{
    c_free(a, hash->keys, hash->num_buckets * (sizeof(*hash->keys) + sizeof(*hash->values)));
    hash->keys = 0;
    hash->values = 0;
    hash->num_buckets = 0;
}