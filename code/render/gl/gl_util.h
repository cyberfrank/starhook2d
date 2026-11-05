#pragma once
#include "foundation/basic.h"

gfx_id create_shader_program(const char *vs_source, const char *fs_source);

gfx_id create_texture_2d(int width, int height, const void *data);
void destroy_texture(gfx_id id);