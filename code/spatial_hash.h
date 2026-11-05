#pragma once
#include "foundation/basic.h"

struct Allocator;

typedef union EntityId {
    struct {
        uint32_t index;
        uint32_t generation;
    };
    uint64_t id;
} EntityId;

typedef struct Entity_Bucket {
    int x;
    int y;
    EntityId *entities;
} Entity_Bucket;

typedef struct Spatial_Hash {
    float cell_size;
    Entity_Bucket **buckets;
    int num_buckets;
    Rect *rects;
    uint32_t *free_slots;
    uint32_t *generation;
    struct Allocator *allocator;
} Spatial_Hash;

Spatial_Hash *spatial_hash_create(float cell_size, int num_buckets, struct Allocator *a);
void spatial_hash_destroy(Spatial_Hash *hash);

EntityId spatial_hash_add(Spatial_Hash *hash, Rect rect);
bool spatial_hash_remove(Spatial_Hash *hash, EntityId e);

EntityId *spatial_hash_get_entities(Spatial_Hash *hash, int x, int y);
void spatial_hash_set_entities(Spatial_Hash *hash, int x, int y, EntityId *entities);

void spatial_hash_query_entities(Spatial_Hash *hash, Rect rect, EntityId **result, struct Allocator *a);

static inline bool is_entity_alive(Spatial_Hash *hash, EntityId e)
{
    return hash->generation[e.index] == e.generation;
}