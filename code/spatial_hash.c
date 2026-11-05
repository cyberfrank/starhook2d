#include "spatial_hash.h"
#include "foundation/array.h"
#include "foundation/math.h"

Spatial_Hash *spatial_hash_create(float cell_size, int num_buckets, struct Allocator *a)
{
    Spatial_Hash *hash = c_alloc(a, sizeof(*hash));
    memset(hash, 0, sizeof(*hash));

    hash->cell_size = cell_size;
    hash->num_buckets = num_buckets;
    hash->buckets = c_alloc(a, sizeof(Entity_Bucket) * num_buckets);
    hash->free_slots = 0;
    hash->generation = 0;
    hash->allocator = a;

    for (int i = 0; i < num_buckets; ++i)
    {
        hash->buckets[i] = 0;
    }

    return hash;
}

void spatial_hash_destroy(Spatial_Hash *hash)
{
    Allocator *a = hash->allocator;
    uint32_t buckets_freed = 0;
    for (int i = 0; i < hash->num_buckets; ++i)
    {
        Entity_Bucket *bucket = hash->buckets[i];
        if (bucket)
        {
            array_free(bucket->entities, a);
            array_free(bucket, a);
            ++buckets_freed;
        }
    }
    c_free(a, hash->buckets, sizeof(Entity_Bucket) * hash->num_buckets);

    array_free(hash->free_slots, a);
    array_free(hash->generation, a);

    c_free(a, hash, sizeof(*hash));
    hash = 0;
}

#define ENTITY_BUCKET_INDEX(num_buckets, x, y) ((uint64_t)((x)*65479+(y)))%(num_buckets)

EntityId *spatial_hash_get_entities(Spatial_Hash *hash, int x, int y)
{
    Entity_Bucket *buckets = hash->buckets[ENTITY_BUCKET_INDEX(hash->num_buckets, x, y)];
    for (int i = 0; i < array_size(buckets); ++i)
    {
        if (buckets[i].x == x && buckets[i].y == y)
        {
            return buckets[i].entities;
        }
    }
    return 0;
}

void spatial_hash_set_entities(Spatial_Hash *hash, int x, int y, EntityId *entities)
{
    const uint64_t index = ENTITY_BUCKET_INDEX(hash->num_buckets, x, y);
    Entity_Bucket *buckets = hash->buckets[index];
    for (int i = 0; i < array_size(buckets); ++i)
    {
        if (buckets[i].x == x && buckets[i].y == y)
        {
            buckets[i].entities = entities;
            return;
        }
    }
    const Entity_Bucket new_bucket = (Entity_Bucket) { 
        .x = x,
        .y = y,
        .entities = entities,
    };
    array_push(buckets, new_bucket, hash->allocator);
    hash->buckets[index] = buckets;
}

EntityId spatial_hash_add(Spatial_Hash *hash, Rect rect)
{
    uint32_t index;
    if (array_size(hash->free_slots) != 0)
    {
        index = array_pop(hash->free_slots);
        hash->rects[index] = rect;
    }
    else
    {
        array_push(hash->generation, 0, hash->allocator);
        index = (uint32_t)array_size(hash->generation) - 1;
        array_push(hash->rects, rect, hash->allocator);
    }

    EntityId e = {
        .index = index,
        .generation = hash->generation[index],
    };

    int x0 = (int)floorf(rect.x / hash->cell_size);
    int y0 = (int)floorf(rect.y / hash->cell_size);
    int x1 = (int)ceilf((rect.x + rect.w) / hash->cell_size);
    int y1 = (int)ceilf((rect.y + rect.h) / hash->cell_size);
    for (int x = x0; x < x1; ++x)
    {
        for (int y = y0; y < y1; ++y)
        {
            EntityId *entities = spatial_hash_get_entities(hash, x, y);
            array_push(entities, e, hash->allocator);
            spatial_hash_set_entities(hash, x, y, entities);
        }
    }

    return e;
}

bool spatial_hash_remove(Spatial_Hash *hash, EntityId e)
{
    if (!is_entity_alive(hash, e))
    {
        return false;
    }

    const Rect rect = hash->rects[e.index];

    int x0 = (int)floorf(rect.x / hash->cell_size);
    int y0 = (int)floorf(rect.y / hash->cell_size);
    int x1 = (int)ceilf((rect.x + rect.w) / hash->cell_size);
    int y1 = (int)ceilf((rect.y + rect.h) / hash->cell_size);
    for (int x = x0; x < x1; ++x)
    {
        for (int y = y0; y < y1; ++y)
        {
            EntityId *entities = spatial_hash_get_entities(hash, x, y);
            for (uint64_t i = 0; i < array_size(entities); ++i)
            {
                if (entities[i].id == e.id)
                {
                    array_remove(entities, 1, i);
                    break;
                }
            }
            spatial_hash_set_entities(hash, x, y, entities);
        }
    }

    ++hash->generation[e.index];
    array_push(hash->free_slots, e.index, hash->allocator);

    return true;
}

static bool rect_intersects(Rect rec1, Rect rec2) 
{
	return rec1.x <= rec2.x + rec2.w && rec1.x + rec1.w >= rec2.x &&
		rec1.y <= rec2.y + rec2.h && rec1.y + rec1.h >= rec2.y;
}

void spatial_hash_query_entities(Spatial_Hash *hash, Rect rect, EntityId **result, struct Allocator *a)
{
    array_reset(*result);

    int x0 = (int)floorf(c_max(0, rect.x / hash->cell_size));
    int y0 = (int)floorf(c_max(0, rect.y / hash->cell_size));
    int x1 = (int)ceilf((rect.x + rect.w) / hash->cell_size);
    int y1 = (int)ceilf((rect.y + rect.h) / hash->cell_size);
    for (int x = x0; x < x1; ++x)
    {
        for (int y = y0; y < y1; ++y)
        {
            EntityId *entities = spatial_hash_get_entities(hash, x, y);
            for (EntityId *it = entities; it != array_end(entities); ++it)
            {
                if (is_entity_alive(hash, *it) && rect_intersects(rect, hash->rects[it->index]))
                {
                    bool unique = true;
                    for (uint64_t i = 0; i < array_size(*result); ++i)
                    {
                        if ((*result)[i].id == it->id)
                        {
                            unique = false;
                            break;
                        }
                    }
                    if (unique)
                    {
                        array_push(*result, *it, a);
                    }
                }
            }
        }
    }
}