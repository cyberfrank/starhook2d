#include "foundation/basic.h"
#include "foundation/os.h"
#include "foundation/array.h"
#include "foundation/log.h"
#include "foundation/math.h"
#include "foundation/atomics.inl"
#include "foundation/asset_catalog.h"
#include "foundation/sprintf.h"
#include "foundation/string_util.h"
#include "render/window.h"
#include "render/sprite_batch.h"
#include "render/texture.h"
#include "render/gl/gl_util.h"
#include "input_system.h"
#include "lua_bindings.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define STARHOOK_LOG_FILE_NAME ("logs/log.txt")
#define STARHOOK_CONFIG_FILE_NAME ("config.lua")
#define STARHOOK_MAIN_FILE_NAME ("main.lua")

typedef struct Log_Message {
    int level;
    char *str;
    uint32_t len;
    double time_to_live;
} Log_Message;

struct Log {
    Allocator *allocator;
    Log_Message *stack;
    File_Handle log_file;
    uint64_t log_file_offset;
} *logger = &(struct Log) {
    0
};

Window_Settings *window_settings = &(Window_Settings) {
    .width = 640,
    .height = 480,
    .scale = 2,
    .title = "No Title",
    .titlebar_color = { 0.1f, 0.1f, 0.1f, 1.f },
    .vsync = false,
};
Asset_Catalog *textures;
Asset_Catalog *sprite_sheets;

void log_screen(int level, const char *msg, uint32_t len)
{
    Log_Message entry = {
        .level = level,
        .str = c_alloc(logger->allocator, len + 1),
        .len = len + 1,
        .time_to_live = 10.0,
    };
    memcpy(entry.str, msg, len);
    entry.str[len] = '\0';
    array_push(logger->stack, entry, logger->allocator);
}

void log_file(int level, const char *msg, uint32_t len)
{
    if (os_write_file_at(logger->log_file, logger->log_file_offset, msg, len))
    {
        logger->log_file_offset += len;
    }
}

// int CALLBACK WinMain(
//     HINSTANCE   hInstance,
//     HINSTANCE   hPrevInstance,
//     LPSTR       lpCmdLine,
//     int         nCmdShow
//     )
int main(int argc, char **argv)
{
    const char *root_path = argc > 1 ? argv[1] : ".";

    char config_file_path[FILE_PATH_MAX_LEN];
    char main_file_path[FILE_PATH_MAX_LEN];
    c_print(config_file_path, FILE_PATH_MAX_LEN, "%s/%s", root_path, STARHOOK_CONFIG_FILE_NAME);
    c_print(main_file_path, FILE_PATH_MAX_LEN, "%s/%s", root_path, STARHOOK_MAIN_FILE_NAME);

    logger->allocator = system_allocator;
    logger->stack = 0;
    logger->log_file = os_open_file_output(STARHOOK_LOG_FILE_NAME);

    // Creates a new lua runtime
    lua_State *L = luaL_newstate();
    // Enable lua standard libraries
    luaL_openlibs(L);

    bool show_log_on_screen = false;
    bool show_fps_on_screen = false;

    if (luaL_dofile(L, config_file_path) == LUA_OK)
    {
        lua_get_int(L, "screen_width", &window_settings->width);
        lua_get_int(L, "screen_height", &window_settings->height);
        lua_get_float(L, "window_scale", &window_settings->scale);
        lua_get_string(L, "window_title", &window_settings->title);
        lua_get_bool(L, "vsync", &window_settings->vsync);
        lua_get_bool(L, "hide_cursor", &window_settings->hide_cursor);
        lua_get_bool(L, "show_log", &show_log_on_screen);
        lua_get_bool(L, "show_fps", &show_fps_on_screen);
    }
    else
    {
        log_warn("No config file was found at path '%s'. Resorting to default settings!", config_file_path);
    }

    register_log_callback(log_stdout);
    register_log_callback(log_file);
    
    if (show_log_on_screen)
    {
        register_log_callback(log_screen);
    }

    log_info("Num cores: %i", os_num_logical_processors());
    log_info("Current thread is %i", os_thread_id());

    void *window = window_setup(window_settings);
    sprite_batch_create();

    textures = make_asset_catalog(4096, &(Asset_Catalog_Interface) {
        .asset_size = sizeof(Texture),
        .descriptor_size = sizeof(Texture_Asset_Descriptor),
        .callbacks = {
            .asset_load = load_texture_asset,
            .asset_load_complete = load_texture_asset_complete,
            .asset_free = free_texture_asset,
        },
    });

    sprite_sheets = make_asset_catalog(4096, &(Asset_Catalog_Interface) {
        .asset_size = sizeof(Sprite_Sheet),
        .descriptor_size = sizeof(Sprite_Sheet_Asset_Descriptor),
        .callbacks = {
            .asset_load = load_sprite_sheet_asset,
            .asset_load_complete = load_sprite_sheet_asset_complete,
        }
    });

    // Load default font
    Asset_Id default_font_id = find_or_load_asset(sprite_sheets, "data/default_font.spritesheet", 0, true);

    Time_Stamp time_since_last_frame = os_time_now();
    double smooth_delta_time = 0.0f;
    double frame_time = 0.0;
    double time_since_start = 0.0;
    uint64_t accumulated_frames = 0;
    float frames_per_second = 0.0f;

    // Register our own bindings
    register_lua_bindings(L);
    // Add default font as global
    lua_pushinteger(L, default_font_id.id);
    lua_setglobal(L, "default_font");

    int status = luaL_dofile(L, main_file_path);
    if (status == LUA_OK)
    {
        lua_generic_call(L, "init", "");
    }
    else
    {
        log_error("%s", lua_tostring(L, -1));
    }

    const Vec4 log_level_colors[] = {
        { 0.8f, 0.8f, 1.f, 1.f }, // Info
        { 1.f, 0.8f, 0.f, 1.f },  // Warn
        { 1.f, 0.f, 0.f, 1.f },   // Error
        { 1.f, 0.f, 0.f, 1.f },   // Trace
        { 1.f, 0.f, 0.f, 1.f },   // Fatal
    };
    const float text_scale = 1.f / window_settings->scale;
    const Rect window_rect = (Rect) { 0.f, 0.f, (float)window_settings->width, (float)window_settings->height };

    while (!is_window_close_requested(window))
    {
        Time_Stamp time_now = os_time_now();
        double delta_time = os_time_delta(time_now, time_since_last_frame);
        time_since_last_frame = time_now;

        poll_async_assets();
        poll_events();
        
        sprite_batch_begin_frame(window_rect);

        if (status == LUA_OK)
        {
            // Update callback
            lua_generic_call(L, "update", "d", delta_time);

            // Render callback
            lua_generic_call(L, "draw", "");
        }

        if (show_log_on_screen)
        {
            Vec2 position = (Vec2) { 0.f, 0.f };
            for (uint64_t i = 0; i < array_size(logger->stack); ++i)
            {
                Log_Message *msg = &logger->stack[i];
                sprite_batch_set_color((Vec4) { 0.f, 0.f, 0.f, 1.f });
                sprite_batch_draw_text(default_font_id, msg->str, vec2_add(position, (Vec2) { 0.5f, 0.5f }), text_scale);
                sprite_batch_set_color(log_level_colors[msg->level]);
                position.y += sprite_batch_draw_text(default_font_id, msg->str, position, text_scale).h;
    
                if (msg->time_to_live <= 0.0)
                {
                    c_free(logger->allocator, msg->str, msg->len);
                    array_remove(logger->stack, 1, i);
                }

                msg->time_to_live -= delta_time;
            }
        }
            
        if (show_fps_on_screen)
        {
            char buf[64];
            int num_chars = c_print(buf, 64, "%.1f fps, %.3f ms", frames_per_second, smooth_delta_time * 1000.f);
            Sprite_Sheet *font = asset_data(sprite_sheets, default_font_id);
            float x_pos = (float)window_settings->width;
            if (font)
            {
                x_pos -= num_chars * font->sprite_width * text_scale;
            }
            if (smooth_delta_time > (1.f / 60.f) * 1.1f) // 10%
            {
                sprite_batch_set_color(log_level_colors[LOG_ERROR]);
            }
            else
            {
                sprite_batch_set_color(log_level_colors[LOG_WARN]);
            }
            sprite_batch_draw_text(default_font_id, buf, make_vec2(x_pos - 4.f, 2.f), text_scale);
        }

        sprite_batch_end_frame();
        
        window_swap_buffers(window);
        
        smooth_delta_time = smooth_delta_time * 0.9f + delta_time * 0.1f;
        frame_time += delta_time;
        time_since_start += delta_time;
        frames_per_second = (float)(accumulated_frames / frame_time);
        if (frame_time >= 1.0)
        {
            frame_time = 0.0;
            accumulated_frames = 0;
        }
        ++accumulated_frames;
    }

    lua_close(L);

    sprite_batch_destroy();
    free_asset_catalog(textures);
    free_asset_catalog(sprite_sheets);
    window_destroy(window);

    log_info("Closing...");

    for (uint32_t i = 0; i < array_size(logger->stack); ++i)
    {
        Log_Message *msg = &logger->stack[i];
        c_free(logger->allocator, msg->str, msg->len);
    }
    array_free(logger->stack, logger->allocator);
    os_close_file(logger->log_file);

    return 0;
}