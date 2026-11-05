#include "texture.h"
#include "foundation/allocator.h"
#include "foundation/asset_catalog.h"
#include "gl/gl_util.h"
#include "foundation/log.h"
#include "foundation/string_util.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool load_sprite_sheet_asset(const void *data, uint64_t size, Sprite_Sheet_Asset_Descriptor *desc)
{
    char *p = (char *)data;
    const char *end = (char *)data + size;
    char scratch_buf[512];
    int scratch_buf_size = 0;

    while (p != end)
    {
        if (*p == ';')
        {
            // Skip comment
            while (p != end && *p != '\n')
            {
                ++p;
            }
        }
        if (string_starts_with("sprite_width:", p))
        {
            scratch_buf_size = 0;
            while (p != end && !isdigit(*p))
            {
                ++p;
            }
            while (p != end && isdigit(*p)) 
            {
                scratch_buf[scratch_buf_size++] = *p++;
            }
            scratch_buf[scratch_buf_size] = 0;
            desc->sprite_width = strtol(scratch_buf, NULL, 10);
        }
        else if (string_starts_with("sprite_height:", p))
        {
            scratch_buf_size = 0;
            while (p != end && !isdigit(*p))
            {
                ++p;
            }
            while (p != end && isdigit(*p)) 
            {
                scratch_buf[scratch_buf_size++] = *p++;
            }
            scratch_buf[scratch_buf_size] = 0;
            desc->sprite_height = strtol(scratch_buf, NULL, 10);
        }
        else if (string_starts_with("path:", p))
        {
            scratch_buf_size = 0;
            while (p != end && *p++ != '\"')
            {
            }
            while (p != end && *p != '\"')
            {
                scratch_buf[scratch_buf_size++] = *p++;
            }
            strncpy_safe(desc->texture_path, scratch_buf, scratch_buf_size);
        }
        else
        {
            ++p;
        }
    }
    return true;
}

void load_sprite_sheet_asset_complete(const Sprite_Sheet_Asset_Descriptor *desc, struct Sprite_Sheet *sheet)
{
    extern Asset_Catalog *textures;
    sheet->texture_id = find_or_load_asset(textures, desc->texture_path, 0, true);
    sheet->sprite_width = desc->sprite_width;
    sheet->sprite_height = desc->sprite_height;
}

bool load_texture_asset(const void *data, uint64_t size, Texture_Asset_Descriptor *desc)
{
    int w, h, c;
    uint8_t *pixels = stbi_load_from_memory(data, (int)size, &w, &h, &c, 4);
    if (pixels == 0)
    {
        return false;
    }
    desc->pixels = pixels;
    desc->width = w;
    desc->height = h;
    return true;
}

void load_texture_asset_complete(const Texture_Asset_Descriptor *desc, Texture *texture)
{
    gfx_id id = create_texture_2d(desc->width, desc->height, desc->pixels);
    *texture = (Texture) {
        .width = desc->width,
        .height = desc->height,
        .id = id,
    };
}

void free_texture_asset(Texture *texture)
{
    if (texture != 0 && texture->id != 0)
    {
        destroy_texture(texture->id);
    }
}