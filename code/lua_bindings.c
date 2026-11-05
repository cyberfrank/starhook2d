#include "lua_bindings.h"
#include "foundation/allocator.h"
#include "foundation/murmur_hash64.h"
#include "foundation/array.h"
#include "foundation/log.h"
#include "foundation/asset_catalog.h"
#include "render/sprite_batch.h"
#include "render/texture.h"
#include "input_system.h"
#include "spatial_hash.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include <math.h>

static int convert_string_to_button(const char *str);

void lua_get_int(lua_State *L, const char *name, int *result) 
{
    lua_getglobal(L, name);
    if (lua_isinteger(L, -1)) 
    {
        *result = (int)lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
}

void lua_get_float(lua_State *L, const char *name, float *result) 
{
    lua_getglobal(L, name);
    if (lua_isnumber(L, -1))
    {
        *result = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 1);
}

void lua_get_string(lua_State *L, const char *name, const char **result)
{
    lua_getglobal(L, name);
    if (lua_isstring(L, -1)) 
    {
        *result = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
}

void lua_get_bool(struct lua_State *L, const char *name, bool *result)
{
    lua_getglobal(L, name);
    if (lua_isboolean(L, -1)) 
    {
        *result = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
}

void lua_generic_call(lua_State *L, const char *func, const char *signature, ...)
{
    va_list vl;

    va_start(vl, signature);
    lua_getglobal(L, func);

    // Push arguments
    int num_args = 0;
    while (*signature)
    {
        switch (*signature++)
        {
            case 'd': // Double
                lua_pushnumber(L, va_arg(vl, double));
                break;
            case 'i': // Integer
                lua_pushnumber(L, va_arg(vl, int));
                break;
            case 's': // String
                lua_pushstring(L, va_arg(vl, char *));
                break;
            case '>':
                // From this point on, we are expecting the result types
                goto endwhile;
            default:
                log_error("Invalid option: %c", *(signature - 1));
                break;
        }
        num_args++;
        luaL_checkstack(L, 1, "Too many arguments");
    } endwhile:

    // Find number of expected results
    int num_results = (int)strlen(signature);
    lua_pcall(L, num_args, num_results, 0);

    // Retrieve results
    num_results = -num_results;
    while (*signature)
    {
        switch (*signature++)
        {
        case 'd': // Double
            if (!lua_isnumber(L, num_results))
            {
                log_error("Wrong result type! Expected 'd' for double");
            }
            *va_arg(vl, double *) = lua_tonumber(L, num_results);
            break;
        case 'i': // Integer
            if (!lua_isnumber(L, num_results))
            {
                log_error("Wrong result type! Expected 'i' for integer");
            }
            *va_arg(vl, int *) = (int)lua_tonumber(L, num_results);
            break;
        case 's': // String
            if (!lua_isstring(L, num_results))
            {
                log_error("Wrong result type! Expected 's' for string");
            }
            *va_arg(vl, const char **) = lua_tostring(L, num_results);
            break;
        default:
            log_error("Invalid option: %c", *(signature - 1));
            break;
        }
        num_results++;
    }
    va_end(vl);
}

static Asset_Id get_asset_id(lua_State *L, int index)
{
    uint64_t id = luaL_optinteger(L, index, INVALID_ASSET_ID);
    return (Asset_Id) { .id = id };
}

static bool get_rect(lua_State *L, int index, Rect *result)
{
    if (lua_istable(L, index))
    {
        lua_rawgeti(L, index, 1);
        result->x = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, index, 2);
        result->y = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, index, 3);
        result->w = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, index, 4);
        result->h = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        return true;
    }
    return false;
}

// Bindings

static int l_Print(lua_State *L)
{
    lua_Debug ar;
    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "Sl", &ar);

    int num_args = lua_gettop(L);
    for (int i = 1; i <= num_args; ++i) 
    {
        if (lua_isstring(L, i)) 
        {
            log_print(LOG_INFO, ar.source, ar.currentline, "%s", lua_tostring(L, i));
        }
    }
    return 0;
}

static int l_SetSpriteColor(lua_State *L)
{
    Vec4 color;
    color.x = (float)luaL_optnumber(L, 1, 1.0);
    color.y = (float)luaL_optnumber(L, 2, 1.0);
    color.z = (float)luaL_optnumber(L, 3, 1.0);
    color.w = (float)luaL_optnumber(L, 4, 1.0);
    sprite_batch_set_color(color);
    return 0;
}

static int l_SetSpriteDepth(lua_State *L)
{
    float depth = (float)luaL_optnumber(L, 1, 0.0);
    sprite_batch_set_depth(depth);
    return 0;
}

static int l_SetSpriteFlip(lua_State *L)
{
    uint8_t flag = (uint8_t)luaL_optnumber(L, 1, 0.0);
    sprite_batch_set_flip(flag);
    return 0;
}

static int l_PushClipRect(lua_State *L)
{
    Rect clip_rect;
    if (get_rect(L, 1, &clip_rect))
    {
        sprite_batch_push_clip_rect(clip_rect);
    }
    return 0;
}

static int l_PopClipRect(lua_State *L)
{
    sprite_batch_pop_clip_rect();
    return 0;
}

static int l_DrawSprite(lua_State *L)
{
    extern Asset_Catalog *textures;

    Vec2 position;
    float rotation;
    Vec2 scale;
    Vec2 origin;

    Asset_Id asset_id = get_asset_id(L, 1);
    Texture *texture = asset_data(textures, asset_id);

    Rect source_rect;
    if (!get_rect(L, 2, &source_rect))
    {
        if (texture != 0)
        {
            // No rect provided: use entire texture instead
            source_rect = (Rect) { 0.f, 0.f, (float)texture->width, (float)texture->height };
        }
        else
        {
            source_rect = (Rect) { 0.f, 0.f, 1.f, 1.f };
        }
    }

    position.x = (float)luaL_optnumber(L, 3, 0.0);
    position.y = (float)luaL_optnumber(L, 4, 0.0);
    rotation   = (float)luaL_optnumber(L, 5, 0.0);
    scale.x    = (float)luaL_optnumber(L, 6, 1.0);
    scale.y    = (float)luaL_optnumber(L, 7, 1.0);
    origin.x   = (float)luaL_optnumber(L, 8, 0.0);
    origin.y   = (float)luaL_optnumber(L, 9, 0.0);

    sprite_batch_draw_sprite(asset_id, source_rect, position, rotation, scale, origin);

    return 0;
}

static int l_DrawSpriteSheet(lua_State *L)
{
    int sprite_x = 0;
    int sprite_y = 0;
    Vec2 position;
    float rotation;
    Vec2 scale;
    Vec2 origin;

    Asset_Id asset_id = get_asset_id(L, 1);

    if (lua_istable(L, 2))
    {
        lua_rawgeti(L, 2, 1);
        sprite_x = (int)luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, 2, 2);
        sprite_y = (int)luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }

    position.x = (float)luaL_optnumber(L, 3, 0.0);
    position.y = (float)luaL_optnumber(L, 4, 0.0);
    rotation   = (float)luaL_optnumber(L, 5, 0.0);
    scale.x    = (float)luaL_optnumber(L, 6, 1.0);
    scale.y    = (float)luaL_optnumber(L, 7, 1.0);
    origin.x   = (float)luaL_optnumber(L, 8, 0.0);
    origin.y   = (float)luaL_optnumber(L, 9, 0.0);

    sprite_batch_draw_sheet(asset_id, sprite_x, sprite_y, position, rotation, scale, origin);

    return 0;
}

static int l_DrawText(lua_State *L)
{
    Vec2 position;
    float scale;

    Asset_Id font_id = get_asset_id(L, 1);

    const char *text = luaL_checkstring(L, 2);

    position.x = (float)luaL_optnumber(L, 3, 0.0);
    position.y = (float)luaL_optnumber(L, 4, 0.0);
    scale = (float)luaL_optnumber(L, 5, 1.0);

    Rect rect = sprite_batch_draw_text(font_id, text, position, scale);
    lua_pushnumber(L, rect.x);
    lua_pushnumber(L, rect.y);
    lua_pushnumber(L, rect.w);
    lua_pushnumber(L, rect.h);
    return 4;
}

static int l_LoadTexture(lua_State *L)
{
    extern Asset_Catalog *textures;
    const char *path = lua_tostring(L, 1);
    Asset_Id asset_id = find_or_load_asset(textures, path, 0, true);
    lua_pushinteger(L, asset_id.id);
    return 1;
}

static int l_LoadSpriteSheet(lua_State *L)
{
    extern Asset_Catalog *sprite_sheets;
    const char *path = lua_tostring(L, 1);
    Asset_Id asset_id = find_or_load_asset(sprite_sheets, path, 0, true);
    lua_pushinteger(L, asset_id.id);
    return 1;
}

static int l_TextureSize(lua_State *L)
{
    extern Asset_Catalog *textures;
    Asset_Id asset_id = get_asset_id(L, 1);
    Texture *texture = asset_data(textures, asset_id);
    if (texture != 0)
    {
        lua_pushinteger(L, texture->width);
        lua_pushinteger(L, texture->height);
    }
    else
    {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }
    return 2;
}

static int l_CreateSpatialHash(lua_State *L)
{
    int num_buckets = 128;
    float cell_size = (float)luaL_optnumber(L, 1, 16.0);
    Spatial_Hash *hash = spatial_hash_create(cell_size, num_buckets, system_allocator);

    lua_pushlightuserdata(L, hash);

    return 1;
}

static int l_DestroySpatialHash(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    if (hash)
    {
        spatial_hash_destroy(hash);
    }
    return 0;
}

static int l_SpatialHashAdd(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    if (hash)
    {
        Rect r;
        r.x = (float)luaL_checknumber(L, 2);
        r.y = (float)luaL_checknumber(L, 3);
        r.w = (float)luaL_checknumber(L, 4);
        r.h = (float)luaL_checknumber(L, 5);
        EntityId entity_id = spatial_hash_add(hash, r);
        lua_pushinteger(L, entity_id.id);
        lua_pushinteger(L, entity_id.index);
        lua_pushinteger(L, entity_id.generation);
        return 3;
    }
    return 0;
}

static int l_SpatialHashRemove(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    bool success = false;
    if (hash)
    {
        EntityId entity_id;
        entity_id.id = (uint64_t)luaL_checkinteger(L, 2);
        success = spatial_hash_remove(hash, entity_id);
    }
    lua_pushboolean(L, success);
    return 1;
}

static void spatial_query_push_table(lua_State *L, Spatial_Hash *hash, Rect r)
{
    Allocator *a = system_allocator;
    EntityId *result = 0;
    spatial_hash_query_entities(hash, r, &result, a);

    lua_createtable(L, (int)array_size(result), 0);
    for (uint64_t i = 0; i < array_size(result); ++i)
    {
        lua_pushinteger(L, result[i].id);
        lua_rawseti(L, -2, i + 1);
    }
    array_free(result, a);
}

static int l_SpatialHashQuery(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    if (hash)
    {
        Rect r;
        r.x = (float)luaL_checknumber(L, 2);
        r.y = (float)luaL_checknumber(L, 3);
        r.w = (float)luaL_checknumber(L, 4);
        r.h = (float)luaL_checknumber(L, 5);
        spatial_query_push_table(L, hash, r);
        return 1;
    }
    return 0;
}

static int l_SpatialHashQueryRadius(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    if (hash)
    {
        float x = (float)luaL_checknumber(L, 2);
        float y = (float)luaL_checknumber(L, 3);
        float r = (float)luaL_checknumber(L, 4);
        Rect rect = (Rect) {
            .x = x - r,
            .y = y - r,
            .w = r * 2.f,
            .h = r * 2.f
        };
        spatial_query_push_table(L, hash, rect);
        return 1;
    }
    return 0;
}

static int l_SpatialEntityAlive(lua_State *L)
{
    Spatial_Hash *hash = lua_touserdata(L, 1);
    bool alive = false;
    if (hash)
    {
        EntityId e;
        e.id = (uint64_t)luaL_checkinteger(L, 2);
        alive |= is_entity_alive(hash, e);
    }
    lua_pushboolean(L, alive);
    return 1;
}

int l_AddInputBinding(lua_State *L) 
{
    const char *str = luaL_checkstring(L, 1);
    int binding_type = -1;
    if (strcmp(str, "action") == 0)
    {
        binding_type = INPUT_BINDING_TYPE__BUTTON;
    }
    else if (strcmp(str, "axis") == 0)
    {
        binding_type = INPUT_BINDING_TYPE__AXIS;
    }

    if (binding_type != -1)
    {
        lua_pushinteger(L, input_add_binding(binding_type));
        return 1;
    }
    else
    {
        log_error("Invalid input binding type '%s': expected either 'action' or 'axis'", str);
        return 0;
    }
}

int l_AddAxisInputMapping(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    int axis = (int)luaL_checkinteger(L, 2);
    float scale = (float)luaL_checknumber(L, 3);
    float bias = (float)luaL_checknumber(L, 4);
    lua_pushboolean(L, input_add_axis_mapping(id, axis, scale, bias));
    return 1;
}

int l_AddActionInputMapping(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    const char *str = luaL_checkstring(L, 2);
    int button = convert_string_to_button(str);
    if (button != -1)
    {
        lua_pushboolean(L, input_add_button_mapping(id, button));
        return 1;
    }
    return 0;
}

int l_GetAxis(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    lua_pushnumber(L, input_get_axis(id));
    return 1;
}

int l_GetButtonDown(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, input_button_down(id));
    return 1;
}

int l_GetButtonPressed(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, input_button_pressed(id));
    return 1;
}

int l_GetButtonReleased(lua_State *L) 
{
    int id = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, input_button_released(id));
    return 1;
}

int l_GetMouseScroll(lua_State *L) 
{
    const Vec2 mouse_scroll = input_mouse_scroll();
    lua_pushnumber(L, mouse_scroll.x);
    lua_pushnumber(L, mouse_scroll.y);
    return 2;
}

static int l_GetMousePosition(lua_State *L)
{
    const Vec2 mouse_position = input_mouse_position();
    lua_pushnumber(L, mouse_position.x);
    lua_pushnumber(L, mouse_position.y);
    return 2;
}

static int l_IsUsingGamepad(lua_State *L)
{
    lua_pushboolean(L, input_is_using_gamepad());
    return 1;
}

#define BIND_C_FUNCTION(L, Name, Func) { lua_pushcfunction(L, Func); lua_setfield(L, -2, Name); }
#define BIND_C_FUNCTION_GLOBAL(L, Name, Func) { lua_pushcfunction(L, Func); lua_setglobal(L, Name); }

void register_lua_bindings(lua_State *L)
{
    lua_newtable(L);
    
    BIND_C_FUNCTION_GLOBAL(L, "print", l_Print);

    lua_newtable(L);
    {
        BIND_C_FUNCTION(L, "load_texture", l_LoadTexture);
        BIND_C_FUNCTION(L, "load_sprite_sheet", l_LoadSpriteSheet);
        BIND_C_FUNCTION(L, "texture_size", l_TextureSize);
        BIND_C_FUNCTION(L, "sprite_color", l_SetSpriteColor);
        BIND_C_FUNCTION(L, "sprite_depth", l_SetSpriteDepth);
        BIND_C_FUNCTION(L, "sprite_flip", l_SetSpriteFlip);
        BIND_C_FUNCTION(L, "push_clip_rect", l_PushClipRect);
        BIND_C_FUNCTION(L, "pop_clip_rect", l_PopClipRect);
        BIND_C_FUNCTION(L, "draw_sprite", l_DrawSprite);
        BIND_C_FUNCTION(L, "draw_sprite_sheet", l_DrawSpriteSheet);
        BIND_C_FUNCTION(L, "draw_text", l_DrawText);
    }
    lua_setglobal(L, "gfx");

    lua_newtable(L);
    {
        BIND_C_FUNCTION(L, "add_input_binding", l_AddInputBinding);
        BIND_C_FUNCTION(L, "add_axis_input_mapping", l_AddAxisInputMapping);
        BIND_C_FUNCTION(L, "add_button_input_mapping", l_AddActionInputMapping);
        BIND_C_FUNCTION(L, "axis", l_GetAxis);
        BIND_C_FUNCTION(L, "button", l_GetButtonDown);
        BIND_C_FUNCTION(L, "button_pressed", l_GetButtonPressed);
        BIND_C_FUNCTION(L, "button_released", l_GetButtonReleased);
        BIND_C_FUNCTION(L, "mouse_position", l_GetMousePosition);
        BIND_C_FUNCTION(L, "mouse_scroll", l_GetMouseScroll);
        BIND_C_FUNCTION(L, "is_gamepad", l_IsUsingGamepad);
    }
    lua_setglobal(L, "input");

    lua_newtable(L);
    {
        BIND_C_FUNCTION(L, "create_spatial_hash", l_CreateSpatialHash);
        BIND_C_FUNCTION(L, "destroy_spatial_hash", l_DestroySpatialHash);
        BIND_C_FUNCTION(L, "add_entity", l_SpatialHashAdd);
        BIND_C_FUNCTION(L, "remove_entity", l_SpatialHashRemove);
        BIND_C_FUNCTION(L, "spatial_query", l_SpatialHashQuery);
        BIND_C_FUNCTION(L, "spatial_query_radius", l_SpatialHashQueryRadius);
        BIND_C_FUNCTION(L, "is_entity_alive", l_SpatialEntityAlive);
    }
    lua_setglobal(L, "entity");
}

static int convert_string_to_button(const char *str)
{
    uint64_t hash = murmur_hash64a_string(str);
    switch (hash)
    {
        case STATIC_HASH("space", 0xb1fc881a8ead9c10ULL): return KEY_SPACE;
        case STATIC_HASH("\'", 0xed1787e7e3c59b80ULL): return KEY_APOSTROPHE;
        case STATIC_HASH(",", 0x7f070016959ab1b5ULL): return KEY_COMMA;
        case STATIC_HASH("-", 0x23fced8d8b1a16b9ULL): return KEY_MINUS;
        case STATIC_HASH(".", 0x223b2df3c7671369ULL): return KEY_PERIOD;
        case STATIC_HASH("/", 0xaf7fd9ec4cacc2a2ULL): return KEY_SLASH;
        case STATIC_HASH("0", 0x4ccb322c4bcf1d63ULL): return KEY_0;
        case STATIC_HASH("1", 0xa5d08d6299c4888ULL): return KEY_1;
        case STATIC_HASH("2", 0x85e449ca4d7dec1ULL): return KEY_2;
        case STATIC_HASH("3", 0xd2aaca16aa05478eULL): return KEY_3;
        case STATIC_HASH("4", 0x1d0385a117d46b44ULL): return KEY_4;
        case STATIC_HASH("5", 0xe85dba4ad5c47776ULL): return KEY_5;
        case STATIC_HASH("6", 0x222d2f0fee5e9d08ULL): return KEY_6;
        case STATIC_HASH("7", 0x98a73cc3900146f0ULL): return KEY_7;
        case STATIC_HASH("8", 0x193eb08744b3a142ULL): return KEY_8;
        case STATIC_HASH("9", 0x470384a52bef7e9eULL): return KEY_9;
        case STATIC_HASH(";", 0xa00b9350ae2c0b96ULL): return KEY_SEMICOLON;
        case STATIC_HASH("=", 0x3249c0fcc759faf4ULL): return KEY_EQUAL;
        case STATIC_HASH("a", 0x71717d2d36b6b11ULL):  return KEY_A;
        case STATIC_HASH("b", 0xea8bfc7d922a2a37ULL): return KEY_B;
        case STATIC_HASH("c", 0xd8788d18ba82e0b5ULL): return KEY_C;
        case STATIC_HASH("d", 0x17dffbc5a8f17839ULL): return KEY_D;
        case STATIC_HASH("e", 0x7115e430772d6a11ULL): return KEY_E;
        case STATIC_HASH("f", 0x5647bce94f90d9aaULL): return KEY_F;
        case STATIC_HASH("g", 0xb5f2c769562d9a23ULL): return KEY_G;
        case STATIC_HASH("h", 0x46be922470df1f1fULL): return KEY_H;
        case STATIC_HASH("i", 0xdb834c3cd9c7c2c7ULL): return KEY_I;
        case STATIC_HASH("j", 0xdce594566b8c31f5ULL): return KEY_J;
        case STATIC_HASH("k", 0x76e009c21a9e845aULL): return KEY_K;
        case STATIC_HASH("l", 0x8f9cc68c43a0a44ULL):  return KEY_L;
        case STATIC_HASH("m", 0x296d4827ee613bdeULL): return KEY_M;
        case STATIC_HASH("n", 0x14abad76e0309edcULL): return KEY_N;
        case STATIC_HASH("o", 0xb754a4f6a509b6a8ULL): return KEY_O;
        case STATIC_HASH("p", 0xc3534b3149af63fcULL): return KEY_P;
        case STATIC_HASH("q", 0x7af8d99b413c664fULL): return KEY_Q;
        case STATIC_HASH("r", 0xeb9e71988f8c8e3dULL): return KEY_R;
        case STATIC_HASH("s", 0xe5db19474a903141ULL): return KEY_S;
        case STATIC_HASH("t", 0xa9f7b29f271e2bf0ULL): return KEY_T;
        case STATIC_HASH("u", 0xd54a870d27426324ULL): return KEY_U;
        case STATIC_HASH("v", 0x39e135b4a2b31d2ULL):  return KEY_V;
        case STATIC_HASH("w", 0x22727cb14c3bb41dULL): return KEY_W;
        case STATIC_HASH("x", 0x8295151af8224269ULL): return KEY_X;
        case STATIC_HASH("y", 0x9a5db2cd2c1fd6ceULL): return KEY_Y;
        case STATIC_HASH("z", 0x88a824e868c7c5efULL): return KEY_Z;
        case STATIC_HASH("[", 0x638f153eb9a7bce1ULL): return KEY_LEFT_BRACKET;
        case STATIC_HASH("\\", 0xa859536933eaaee3ULL): return KEY_BACKSLASH;
        case STATIC_HASH("]", 0x62e9a76ca190d2baULL): return KEY_RIGHT_BRACKET;
        case STATIC_HASH("`", 0xa52be5b3f6674b2aULL): return KEY_GRAVE_ACCENT;
        // KEY_WORLD_1, KEY_WORLD_2 omitted
        case STATIC_HASH("escape", 0x35aad053f426bc8ULL): return KEY_ESCAPE;
        case STATIC_HASH("enter", 0xbced2f488d897692ULL): return KEY_ENTER;
        case STATIC_HASH("tab", 0x468733a52d09a364ULL): return KEY_TAB;
        case STATIC_HASH("backspace", 0x5497875099ca9b2aULL): return KEY_BACKSPACE;
        case STATIC_HASH("insert", 0xd1f8bec96ae7148ULL): return KEY_INSERT;
        case STATIC_HASH("delete", 0x57c7233fb6473601ULL): return KEY_DELETE;
        case STATIC_HASH("right", 0xc6ef9eb8bc8afb11ULL): return KEY_RIGHT;
        case STATIC_HASH("left", 0xe7add5f891f566bULL): return KEY_LEFT;
        case STATIC_HASH("down", 0xf1884192329514dcULL): return KEY_DOWN;
        case STATIC_HASH("up", 0x4ddc2abd07e1403bULL): return KEY_UP;
        case STATIC_HASH("pageup", 0xd32b4399439a3ef4ULL): return KEY_PAGE_UP;
        case STATIC_HASH("pagedown", 0xe1cda279cd2c4492ULL): return KEY_PAGE_DOWN;
        case STATIC_HASH("home", 0xdaf2790e014b3430ULL): return KEY_HOME;
        case STATIC_HASH("end", 0xe783d2bde6a95d87ULL): return KEY_END;
        case STATIC_HASH("capslock", 0x9ff62367edc62f8fULL): return KEY_CAPS_LOCK;
        case STATIC_HASH("numlock", 0xb7665f871ea01277ULL): return KEY_NUM_LOCK;
        case STATIC_HASH("printscreen", 0x245fc84267a7c2e9ULL): return KEY_PRINT_SCREEN;
        case STATIC_HASH("pause", 0xb2fcf3a0c36204deULL): return KEY_PAUSE;
        case STATIC_HASH("f1", 0xba7e0b93b7ff4b3eULL): return KEY_F1;
        case STATIC_HASH("f2", 0x7bb03d422d9b7832ULL): return KEY_F2;
        case STATIC_HASH("f3", 0x994d302c9302fc6dULL): return KEY_F3;
        case STATIC_HASH("f4", 0x1b32ee9616f252d4ULL): return KEY_F4;
        case STATIC_HASH("f5", 0x6e5e947c8bf5a4ebULL): return KEY_F5;
        case STATIC_HASH("f6", 0x4fc6db21251d20f6ULL): return KEY_F6;
        case STATIC_HASH("f7", 0x3e4791baa1233b0ULL): return KEY_F7;
        case STATIC_HASH("f8", 0xeaabdc00b64df98dULL): return KEY_F8;
        case STATIC_HASH("f9", 0x53131fae94d5c5d9ULL): return KEY_F9;
        case STATIC_HASH("f10", 0x472f539e09447aafULL): return KEY_F10;
        case STATIC_HASH("f11", 0xc1bd7f72169cc979ULL): return KEY_F11;
        case STATIC_HASH("f12", 0x999e26862e3256e3ULL): return KEY_F12;
        case STATIC_HASH("f13", 0xe72717b98b46351cULL): return KEY_F13;
        case STATIC_HASH("f14", 0x2e0e5f283e8e87f4ULL): return KEY_F14;
        case STATIC_HASH("f15", 0x21a81994997633aaULL): return KEY_F15;
        case STATIC_HASH("f16", 0x5512247bddbad552ULL): return KEY_F16;
        case STATIC_HASH("f17", 0x2fac2598b4a4192dULL): return KEY_F17;
        case STATIC_HASH("f18", 0xf40f90255d367828ULL): return KEY_F18;
        case STATIC_HASH("f19", 0xbf57589b2902c324ULL): return KEY_F19;
        case STATIC_HASH("f20", 0x371c78bde4f5e339ULL): return KEY_F20;
        case STATIC_HASH("f21", 0xbfb48c44e8a7622ULL): return KEY_F21;
        case STATIC_HASH("f22", 0x521bdc3908306407ULL): return KEY_F22;
        case STATIC_HASH("f23", 0x5e4754aa8457cd79ULL): return KEY_F23;
        case STATIC_HASH("f24", 0x7fbde672d0534483ULL): return KEY_F24;
        case STATIC_HASH("f25", 0xcf768ce589f72f72ULL): return KEY_F25;
        // KEY_KP_* (320-340) omitted here for now
        case STATIC_HASH("lshift", 0xd25f500406b2a509ULL): return KEY_LEFT_SHIFT;
        case STATIC_HASH("lcontrol", 0x737cb678bce23241ULL): return KEY_LEFT_CONTROL;
        case STATIC_HASH("lalt", 0x6be32b19e6dd4c0fULL): return KEY_LEFT_ALT;
        case STATIC_HASH("lsuper", 0xa01daa278cf2623eULL): return KEY_LEFT_SUPER;
        case STATIC_HASH("rshift", 0xc95ab9600f2d59ccULL): return KEY_RIGHT_SHIFT;
        case STATIC_HASH("rcontrol", 0xf17fd721f7246591ULL): return KEY_RIGHT_CONTROL;
        case STATIC_HASH("ralt", 0xdbe5debd3c69c14ULL): return KEY_RIGHT_ALT;
        case STATIC_HASH("rsuper", 0xf9391f180ba38569ULL): return KEY_RIGHT_SUPER;
        case STATIC_HASH("menu", 0x45cb22eff56352deULL): return KEY_MENU;
    }
    return -1;
}