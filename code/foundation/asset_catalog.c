#include "asset_catalog.h"
#include "hash.h"
#include "allocator.h"
#include "murmur_hash64.h"
#include "array.h"
#include "os.h"
#include "log.h"

struct Asset_Catalog {
    void *data;
    uint64_t asset_size;
    uint64_t descriptor_size;
    uint64_t size;
    uint64_t capacity;
    uint32_t *free_slots;
    uint32_t *generation;
    uint64_t *tags;
    uint64_t *names;
    Hash name_to_index;
    Asset_Catalog_Callbacks callbacks;
    Allocator data_allocator;
    Allocator hash_allocator;
    Allocator generic_allocator;
    bool no_descriptor;
    Asset_Id placeholder_asset;
    Asset_Id fallback_asset;
};

typedef enum Asset_State {
    ASSET_STATE_PENDING,
    ASSET_STATE_VALID,
    ASSET_STATE_FAILED,
    ASSET_STATE_HANDLED,
} Asset_State;

typedef struct Pending_Asset {
    Asset_Catalog *catalog;
    Asset_Id asset_id;
    Allocator *allocator;
    void *descriptor;
    void *data;
    uint64_t size;
    Asset_State asset_state;
} Pending_Asset;

struct Asset_Loader {
    bool initialized;
    Allocator *allocator;
    Thread_Handle thread_handle;
    Semaphore_Handle pending_assets_counter;
    Semaphore_Handle loaded_assets_counter;
    Critical_Section pending_assets_cs;
    Pending_Asset *pending_assets;
} *asset_loader = &(struct Asset_Loader) {
    .initialized = false,
};

static void asset_loader_thread_entry_point(struct Asset_Loader *ctx)
{
    while (true)
    {
        os_semaphore_wait(ctx->pending_assets_counter);

        Pending_Asset asset;
        uint64_t asset_index;

        os_enter_critical_section(&ctx->pending_assets_cs);
        {
            asset_index = array_size(ctx->pending_assets) - 1;
            asset = ctx->pending_assets[asset_index];
        }
        os_leave_critical_section(&ctx->pending_assets_cs);

        Asset_Catalog_Callbacks *callbacks = &asset.catalog->callbacks;
        bool success = asset.data && callbacks->asset_load(asset.data, asset.size, asset.descriptor);

        os_enter_critical_section(&ctx->pending_assets_cs);
        {
            Pending_Asset *pending = &ctx->pending_assets[asset_index];
            pending->asset_state = success ? ASSET_STATE_VALID : ASSET_STATE_FAILED;
            pending->descriptor = asset.descriptor;
        }
        os_leave_critical_section(&ctx->pending_assets_cs);

        os_semaphore_add(ctx->loaded_assets_counter, 1);
    }
}

static inline bool private__is_asset_valid(Asset_Catalog *catalog, Asset_Id asset_id)
{
    return asset_id.id != INVALID_ASSET_ID && asset_id.generation == catalog->generation[asset_id.index];
}

static Asset_Id private__name_to_asset_id(Asset_Catalog *catalog, uint64_t name_hash)
{
    const uint64_t index = hash_get_default(&catalog->name_to_index, name_hash, INVALID_ASSET_ID);
    if (index != INVALID_ASSET_ID && catalog->names[index] == name_hash)
    {
        Asset_Id asset_id = {
            .index = (uint32_t)index,
            .generation = catalog->generation[index],
        };
        return asset_id;
    }
    return (Asset_Id) { .id = INVALID_ASSET_ID };
}

static void private__ensure_buffer_capacity(Asset_Catalog *catalog, uint64_t items_to_add)
{
    uint64_t to_at_least = catalog->size + items_to_add;
    if (to_at_least > catalog->capacity) 
    {
        Allocator *a = &catalog->data_allocator;
        const uint64_t min_capacity = catalog->capacity ? catalog->capacity * 2 : 16;
        const uint64_t new_capacity = min_capacity > to_at_least ? min_capacity : to_at_least;
        const uint64_t bytes_before = catalog->data ? catalog->asset_size * catalog->capacity : 0;
        const uint64_t bytes_after = catalog->asset_size * new_capacity;
        catalog->data = a->allocate_func(a, catalog->data, bytes_before, bytes_after, __FILE__, __LINE__);
        catalog->capacity = new_capacity;
    }
}

static Asset_Id private__make_asset(Asset_Catalog *catalog, uint64_t name_hash, uint64_t tag_hash)
{
    uint32_t index;

    if (array_size(catalog->free_slots) > 0)
    {
        index = array_pop(catalog->free_slots);
    }
    else
    {
        array_push(catalog->generation, 0, &catalog->generic_allocator);
        array_push(catalog->tags, 0, &catalog->generic_allocator);
        array_push(catalog->names, 0, &catalog->generic_allocator);
        private__ensure_buffer_capacity(catalog, 1);
        index = (uint32_t)catalog->size;
        ++catalog->size;
    }

    const Asset_Id asset_id = {
        .index = index,
        .generation = catalog->generation[index],
    };

    if (name_hash)
    {
        catalog->names[index] = name_hash;
        hash_add(&catalog->name_to_index, name_hash, index, &catalog->hash_allocator);
    }

    if (tag_hash)
    {
        catalog->tags[index] = tag_hash;
    }

    return asset_id;
}

static void private__free_asset(Asset_Catalog *catalog, uint32_t index)
{
    void *asset = (uint8_t *)catalog->data + catalog->asset_size * index;

    if (catalog->placeholder_asset.index == index)
    {
        log_error("Cannot free placeholder asset (%zu)!", catalog->names[index]);
        return;
    }

    if (catalog->fallback_asset.index == index)
    {
        log_error("Cannot free fallback asset (%zu)!", catalog->names[index]);
        return;
    }

    if (asset)
    {
        Asset_Catalog_Callbacks *callbacks = &catalog->callbacks;
        if (callbacks->asset_free)
        {
            callbacks->asset_free(asset);
        }
        memset(asset, 0, catalog->asset_size);
    }

    hash_remove(&catalog->name_to_index, catalog->names[index]);
    catalog->generation[index] += 1;
    catalog->tags[index] = 0;
    catalog->names[index] = 0;
    array_push(catalog->free_slots, index, &catalog->generic_allocator);
}

Asset_Catalog *make_asset_catalog(uint64_t reserve_count, Asset_Catalog_Interface *i)
{
    if (!asset_loader->initialized)
    {
        asset_loader->allocator = system_allocator;
        asset_loader->thread_handle = os_create_thread(asset_loader_thread_entry_point, asset_loader, MB(4));
        asset_loader->pending_assets_counter = os_create_semaphore(0);
        asset_loader->loaded_assets_counter = os_create_semaphore(0);
        os_create_critical_section(&asset_loader->pending_assets_cs);
        asset_loader->initialized = true;
    }

    Asset_Catalog *c = c_alloc(system_allocator, sizeof(*c));
    const uint64_t reserve_size = ALIGN_SIZE(i->asset_size * reserve_count, PAGE_SIZE);
    c->data_allocator = allocator_create_fixed_vm(reserve_size);
    c->hash_allocator = *system_allocator;
    c->generic_allocator = *system_allocator;
    c->asset_size = i->asset_size;
    c->descriptor_size = i->no_descriptor ? i->asset_size : i->descriptor_size;
    c->size = 0;
    c->capacity = 0;
    c->data = 0;
    c->free_slots = 0;
    c->generation = 0;
    c->tags = 0;
    c->names = 0;
    c->name_to_index = (Hash) { 0 };
    c->callbacks = i->callbacks;
    c->no_descriptor = i->no_descriptor;
    c->placeholder_asset = (Asset_Id) { .id = INVALID_ASSET_ID };
    c->fallback_asset = (Asset_Id) { .id = INVALID_ASSET_ID };

    return c;
}

void set_placeholder_asset(Asset_Catalog *catalog, Asset_Id asset_id)
{
    catalog->placeholder_asset = asset_id;
}

void set_fallback_asset(Asset_Catalog *catalog, Asset_Id asset_id)
{
    catalog->fallback_asset = asset_id;
}

void free_asset_catalog(Asset_Catalog *catalog)
{
    Asset_Catalog_Callbacks *callbacks = &catalog->callbacks;
    if (callbacks->asset_free) 
    {
        for (uint32_t i = 0; i < catalog->size; ++i) 
        {
            void *asset = (uint8_t *)catalog->data + catalog->asset_size * i;
            if (asset)
            {
                callbacks->asset_free(asset);
            }
        }
    }

    array_free(catalog->free_slots, &catalog->generic_allocator);
    array_free(catalog->generation, &catalog->generic_allocator);
    array_free(catalog->tags, &catalog->generic_allocator);
    array_free(catalog->names, &catalog->generic_allocator);
    hash_free(&catalog->name_to_index, &catalog->hash_allocator);
    c_free(&catalog->data_allocator, catalog->data, catalog->asset_size * catalog->capacity);
    c_free(system_allocator, catalog, sizeof(*catalog));
}

void free_asset(Asset_Catalog *catalog, Asset_Id asset_id)
{
    if (private__is_asset_valid(catalog, asset_id))
    {
        private__free_asset(catalog, asset_id.index);
    }
}

void free_assets_by_tag(Asset_Catalog *catalog, const char *tag)
{
    const uint64_t tag_hash = murmur_hash64a_string(tag);
    for (uint32_t i = 0; i < (uint32_t)array_size(catalog->tags); ++i)
    {
        if (catalog->tags[i] == tag_hash)
        {
            // The tag is reset to `0` when the asset is invalidated so we are safe to assume that
            // all assets that have a tag still are valid
            private__free_asset(catalog, i);
        }
    }
}

Asset_Id find_or_load_asset(Asset_Catalog *catalog, const char *path, const char *tag, bool load_async)
{
    const uint64_t name_hash = murmur_hash64a_string(path);
    const uint64_t tag_hash = murmur_hash64a_string(tag);

    Asset_Id found_asset = private__name_to_asset_id(catalog, name_hash);
    if (private__is_asset_valid(catalog, found_asset))
    {
        if (tag_hash)
        {
            catalog->tags[found_asset.index] = tag_hash; // Update tag
        }
        return found_asset;
    }

    log_info("Begin attempt to load asset '%s' [async=%i]", path, (int)load_async);
    
    Asset_Catalog_Callbacks *callbacks = &catalog->callbacks;
    if (callbacks->asset_load == 0 || (callbacks->asset_load_complete == 0 && !catalog->no_descriptor))
    {
        log_error("Cannot load asset '%s' due to missing load callback", path);
        return (Asset_Id) { .id = INVALID_ASSET_ID };
    }

    Asset_Id asset_id = private__make_asset(catalog, name_hash, tag_hash);
    Allocator *asset_allocator = system_allocator;

    void *asset = asset_data(catalog, asset_id);

    if (load_async)
    {
        uint64_t size = 0;
        void *data = os_read_entire_file(path, &size, asset_allocator);
        
        Pending_Asset pending_asset;
        pending_asset.catalog = catalog;
        pending_asset.asset_id = asset_id;
        pending_asset.allocator = asset_allocator;
        pending_asset.data = data;
        pending_asset.size = size;
        pending_asset.asset_state = ASSET_STATE_PENDING;
        pending_asset.descriptor = c_alloc(asset_allocator, catalog->descriptor_size);

        os_enter_critical_section(&asset_loader->pending_assets_cs);
        {
            array_push(asset_loader->pending_assets, pending_asset, asset_loader->allocator);
        }
        os_leave_critical_section(&asset_loader->pending_assets_cs);

        if (private__is_asset_valid(catalog, catalog->placeholder_asset))
        {
            void *placeholder = asset_data(catalog, catalog->placeholder_asset);
            memcpy(asset, placeholder, catalog->asset_size);
        }

        os_semaphore_add(asset_loader->pending_assets_counter, 1);
    }
    else
    {  
        uint64_t size = 0;
        void *data = os_read_entire_file(path, &size, asset_allocator);
        void *descriptor = c_alloc(asset_allocator, catalog->descriptor_size);
        bool success = data && callbacks->asset_load(data, size, descriptor);
        if (success)
        {
            log_info("Loaded asset '%s' successfully", path);
            if (catalog->no_descriptor)
            {
                memcpy(asset, descriptor, catalog->descriptor_size);
            }
            else
            {
                callbacks->asset_load_complete(descriptor, asset);
            }
        }
        else
        {
            log_error("Failed to load asset '%s'", path);
            if (private__is_asset_valid(catalog, catalog->fallback_asset))
            {
                void *fallback = asset_data(catalog, catalog->fallback_asset);
                memcpy(asset, fallback, catalog->asset_size);
            }
        }
        c_free(asset_allocator, descriptor, catalog->descriptor_size);
        c_free(asset_allocator, data, size);
    }
    return asset_id;
}

Asset_Id find_or_make_asset(Asset_Catalog *catalog, const char *name, const char *tag)
{
    const uint64_t name_hash = murmur_hash64a_string(name);
    const uint64_t tag_hash = murmur_hash64a_string(tag);

    Asset_Id found_asset = private__name_to_asset_id(catalog, name_hash);
    if (private__is_asset_valid(catalog, found_asset))
    {
        if (tag_hash)
        {
            catalog->tags[found_asset.index] = tag_hash; // Update tag
        }
        return found_asset;
    }
    return private__make_asset(catalog, name_hash, tag_hash);
}

void *asset_data(Asset_Catalog *catalog, Asset_Id asset_id)
{
    if (private__is_asset_valid(catalog, asset_id))
    {
        return (uint8_t *)catalog->data + catalog->asset_size * asset_id.index;
    }
    return 0;
}

void poll_async_assets()
{
    if (!asset_loader->initialized)
    {
        return;
    }

    if (!os_semaphore_poll(asset_loader->loaded_assets_counter))
    {
        return;
    }

    Pending_Asset *loaded_assets = 0;

    os_enter_critical_section(&asset_loader->pending_assets_cs);
    {
        bool all_handled = true;
        for (Pending_Asset *it = asset_loader->pending_assets; it != array_end(asset_loader->pending_assets); ++it)
        {
            if (it->asset_state == ASSET_STATE_PENDING)
            {
                all_handled = false;
                continue;
            }
            if (it->asset_state != ASSET_STATE_HANDLED)
            {
                // Copy asset to prevent deadlock in case the load callback wants to load more assets
                array_push(loaded_assets, *it, system_allocator);
                it->asset_state = ASSET_STATE_HANDLED;
            }
        }
        if (all_handled)
        {
            array_reset(asset_loader->pending_assets);
        }
    }
    os_leave_critical_section(&asset_loader->pending_assets_cs);

    for (Pending_Asset *it = loaded_assets; it != array_end(loaded_assets); ++it)
    {
        Asset_Catalog *catalog = it->catalog;
        Asset_Catalog_Callbacks *callbacks = &catalog->callbacks;

        void *asset = asset_data(it->catalog, it->asset_id);
        fatal_check(asset);

        if (it->asset_state == ASSET_STATE_VALID)
        {
            log_info("Loaded async asset %zu successfully", it->asset_id);
            if (catalog->no_descriptor)
            {
                memcpy(asset, it->descriptor, catalog->descriptor_size);
            }
            else
            {
                callbacks->asset_load_complete(it->descriptor, asset);
            }
        }
        else if (it->asset_state == ASSET_STATE_FAILED)
        {
            log_info("Failed to load async asset %zu", it->asset_id);
            if (private__is_asset_valid(catalog, catalog->fallback_asset))
            {
                void *fallback = asset_data(catalog, catalog->fallback_asset);
                memcpy(asset, fallback, catalog->asset_size);
            }
        }
        c_free(it->allocator, it->descriptor, catalog->descriptor_size);
        c_free(it->allocator, it->data, it->size);
    }

    array_free(loaded_assets, system_allocator);
}