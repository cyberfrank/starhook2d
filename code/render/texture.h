#pragma once
#include "foundation/basic.h"

typedef struct Texture {
    int width;
    int height;
    gfx_id id;
} Texture;

typedef struct Texture_Asset_Descriptor {
    uint8_t *pixels;
    int width;
    int height;
} Texture_Asset_Descriptor;

typedef struct Sprite_Sheet {
    Asset_Id texture_id;
    int sprite_width;
    int sprite_height;
} Sprite_Sheet;

typedef struct Sprite_Sheet_Asset_Descriptor {
    char texture_path[260];
    int sprite_width;
    int sprite_height;
} Sprite_Sheet_Asset_Descriptor;

bool load_sprite_sheet_asset(const void *data, uint64_t size, Sprite_Sheet_Asset_Descriptor *desc);
void load_sprite_sheet_asset_complete(const Sprite_Sheet_Asset_Descriptor *desc, struct Sprite_Sheet *sheet);

bool load_texture_asset(const void *data, uint64_t size, Texture_Asset_Descriptor *desc);
void load_texture_asset_complete(const Texture_Asset_Descriptor *desc, struct Texture *texture);
void free_texture_asset(Texture *texture);