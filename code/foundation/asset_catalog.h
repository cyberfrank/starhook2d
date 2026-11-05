#pragma once
#include "basic.h"

struct Allocator;

typedef struct Asset_Catalog Asset_Catalog;

typedef struct Asset_Catalog_Callbacks {
    // Load callback that runs on the background thread
    bool (*asset_load)(const void *data, uint64_t size, void *descriptor);

    // Called when a async load is complete and valid (on main thread)
    void (*asset_load_complete)(const void *descriptor, void *asset);
    
    // Called when assets are being freed
    void (*asset_free)(void *asset);
} Asset_Catalog_Callbacks;

typedef struct Asset_Catalog_Interface {
    uint64_t asset_size;
    uint64_t descriptor_size;
    bool no_descriptor; // True if the asset and the descriptor are the same type
    Asset_Catalog_Callbacks callbacks;
} Asset_Catalog_Interface;

// Create a new asset catalog that can fit up to `reserve_count` assets
Asset_Catalog *make_asset_catalog(uint64_t reserve_count, Asset_Catalog_Interface *i);

// Sets the placeholder asset
void set_placeholder_asset(Asset_Catalog *catalog, Asset_Id asset_id);

// Sets the fallback asset
void set_fallback_asset(Asset_Catalog *catalog, Asset_Id asset_id);

// Free catalog and all assets using the `asset_free()` callback
void free_asset_catalog(Asset_Catalog *catalog);

// Free asset mapped to `asset_id`.
void free_asset(Asset_Catalog *catalog, Asset_Id asset_id);

// Free all assets mapped to `tag`.
void free_assets_by_tag(Asset_Catalog *catalog, const char *tag);

// Returns the asset mapped to `path` if already allocated, otherwise attempts to load it using the `asset_load()` callback.
// Optionally allows mapping the asset to the `tag`. The asset's tag will be updated in case an existing asset was bound to a different tag.
// If `load_async` is true, the loading will be handled on a background thread, and the results need to be polled with `poll_async_assets()`.
Asset_Id find_or_load_asset(Asset_Catalog *catalog, const char *path, const char *tag, bool load_async);

// Returns the asset mapped to `name` if already allocated, otherwise allocates a new asset.
// Optionally allows mapping the asset to the `tag`. The asset's tag will be updated in case an existing asset was bound to a different tag.
Asset_Id find_or_make_asset(Asset_Catalog *catalog, const char *name, const char *tag);

// Get the asset data mapped to `asset_id`. Returns `0` if there's no valid data.
void *asset_data(Asset_Catalog *catalog, Asset_Id asset_id);

// Poll in-flight assets to check whether they are ready to be added to the asset catalogs on the main thread.
void poll_async_assets();
