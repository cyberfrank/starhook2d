#include "sprite_batch.h"
#include "foundation/math.h"
#include "foundation/array.h"
#include "foundation/log.h"
#include "foundation/rect.h"
#include "foundation/asset_catalog.h"
#include "gl/gl_util.h"
#include "gl/glad.h"
#include "texture.h"

const char *sprite_vs = "\
    #version 430 core\n\
    layout (location = 0) in vec3 position;\
    layout (location = 1) in uint color;\
    layout (location = 2) in vec4 clip;\
    layout (location = 3) in vec2 uv;\
    layout (location = 4) in uint texture_slot;\
    uniform mat4 transform;\
    out vec2 out_position;\
    out vec4 out_color;\
    out vec4 out_clip;\
    out vec2 out_uv;\
    flat out uint out_slot;\
    vec4 decode_rgba(uint c) {\
        return vec4((c >> 24) & 0xff, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff) / 255.f;\
    }\
    void main() {\
        out_position = position.xy;\
        out_uv = uv;\
        out_color = decode_rgba(color);\
        out_clip = clip;\
        out_clip.zw += out_clip.xy;\
        out_slot = texture_slot;\
        gl_Position = transform * vec4(position, 1.0);\
    }";

const char *sprite_fs = "\
    #version 430 core\n\
    in vec2 out_position;\
    in vec4 out_color;\
    in vec4 out_clip;\
    in vec2 out_uv;\
    flat in uint out_slot;\
    out vec4 FragColor;\
    layout (binding = 0) uniform sampler2D textures[8];\
    vec4 sample_slot(uint slot, vec2 uv) {\
        switch (slot) {\
            case 0: return texture(textures[0], uv);\
            case 1: return texture(textures[1], uv);\
            case 2: return texture(textures[2], uv);\
            case 3: return texture(textures[3], uv);\
            case 4: return texture(textures[4], uv);\
            case 5: return texture(textures[5], uv);\
            case 6: return texture(textures[6], uv);\
            case 7: return texture(textures[7], uv);\
        }\
        return vec4(1.0);\
    }\
    void main() {\
        if (out_position.x < out_clip.x || out_position.x > out_clip.z || out_position.y < out_clip.y || out_position.y > out_clip.w)\
            discard;\
        vec4 color = out_color;\
        if (out_slot != 0xff)\
            color *= sample_slot(out_slot, out_uv);\
        if (color.a < 0.1f)\
            discard;\
        FragColor = color;\
    }";

enum {
    MAX_BATCH_SIZE = 2048,
    NUM_VERTICES_PER_SPRITE = 4,
    NUM_INDICES_PER_SPRITE = 6,
    MAX_TEXTURE_SLOTS = 8,
    UNTEXTURED_SLOT = 0xff,
};

typedef struct Sprite_Vertex {
    float x, y, depth;
    uint32_t color;
    Rect clip;
    Vec2 uv;
    uint32_t texture_slot;
} Sprite_Vertex;

typedef struct Sprite_Info {
    Vec2 dest_position;
    Vec2 dest_size;
    Vec2 source_position;
    Vec2 source_size;
    Vec2 origin;
    float rotation;
    float depth;
    gfx_id texture_id;
    uint32_t clip_id;
    uint32_t color;
    uint8_t flip_flag;
} Sprite_Info;

struct Sprite_Batch {
    Sprite_Info *sprites;
    Allocator *sprite_allocator;
    Allocator *clip_rect_allocator;
    Rect *clip_rects;
    uint32_t *clip_stack;

    Mat44 projection_matrix;
    int transform_location;
    Rect last_viewport;

    float depth;
    uint8_t flip_flag;
    uint32_t color;

    gfx_id vertex_buffer;
    gfx_id index_buffer;
    gfx_id shader;

    bool initialized;
} *batch = &(struct Sprite_Batch) {
    0
};

extern Asset_Catalog *sprite_sheets;
extern Asset_Catalog *textures;

static gfx_id create_vertex_buffer()
{
    const uint64_t vbuffer_size = sizeof(Sprite_Vertex) * MAX_BATCH_SIZE * NUM_VERTICES_PER_SPRITE;

    gfx_id vbuffer_id;
    glGenBuffers(1, &vbuffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, vbuffer_id);
    glBufferData(GL_ARRAY_BUFFER, vbuffer_size, 0, GL_DYNAMIC_DRAW);

    return vbuffer_id;
}

static gfx_id create_index_buffer()
{
    STATIC_ASSERT((MAX_BATCH_SIZE * NUM_VERTICES_PER_SPRITE) < UINT16_MAX);

    const uint64_t ibuffer_size = sizeof(uint16_t) * MAX_BATCH_SIZE * NUM_INDICES_PER_SPRITE;
    uint16_t *buffer = c_alloc(system_allocator, ibuffer_size);
    uint16_t *p = buffer;

    // Add indices for each quad
    for (uint16_t quad_idx = 0; quad_idx < MAX_BATCH_SIZE * NUM_VERTICES_PER_SPRITE; quad_idx += NUM_VERTICES_PER_SPRITE)
    {
        *p++ = quad_idx + 0;
        *p++ = quad_idx + 1;
        *p++ = quad_idx + 2;
        *p++ = quad_idx + 1;
        *p++ = quad_idx + 3;
        *p++ = quad_idx + 2;
    }

    gfx_id ibuffer_id;
    glGenBuffers(1, &ibuffer_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuffer_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibuffer_size, buffer, GL_STATIC_DRAW);

    c_free(system_allocator, buffer, ibuffer_size);

    return ibuffer_id;
}

typedef struct Mat22 {
    float xx, xy;
    float yx, yy;
} Mat22;

static inline Vec2 mat22_transform(const Mat22 m, Vec2 v)
{
    return (Vec2) {
        m.xx * v.x + m.xy * v.y,
        m.yx * v.x + m.yy * v.y,
    };
}

static inline const Mat22 *mat22_identity()
{
    static Mat22 res = { 
        1, 0,
        0, 1,
    };
    return &res;
}

static inline void orthographic_projection_from_frustum(Mat44 *m, float left, float right, float bottom, float top, float near_plane, float far_plane)
{
    const float width = right - left;
    const float height = top - bottom;
    const float depth = far_plane - near_plane;
    memset(m, 0, sizeof(*m));
    m->xx = 2.f / width;
    m->yy = 2.f / height;
    m->zz = -2.f / depth;
    m->wx = -(right + left) / width;
    m->wy = -(top + bottom) / height;
    m->wz = -(far_plane + near_plane) / depth;
    m->ww = 1.f;
}

void sprite_batch_create()
{
    if (batch->initialized)
    {
        return;
    }

    batch->shader = create_shader_program(sprite_vs, sprite_fs);
    batch->vertex_buffer = create_vertex_buffer();
    batch->index_buffer = create_index_buffer();
    batch->sprites = 0;
    batch->clip_rects = 0;
    batch->clip_stack = 0;
    batch->sprite_allocator = system_allocator;
    batch->clip_rect_allocator = system_allocator;
    batch->color = 0xffffffff;
    batch->depth = 0.f;
    batch->flip_flag = SPRITE_FLIP_NONE;

    batch->transform_location = glGetUniformLocation(batch->shader, "transform");

    batch->initialized = true;
}

void sprite_batch_destroy()
{
    if (!batch->initialized)
    {
        return;
    }

    array_free(batch->sprites, batch->sprite_allocator);
    array_free(batch->clip_rects, batch->clip_rect_allocator);
    array_free(batch->clip_stack, batch->clip_rect_allocator);
    glDeleteProgram(batch->shader);
    glDeleteBuffers(1, &batch->vertex_buffer);
    glDeleteBuffers(1, &batch->index_buffer);
    
    batch->initialized = false;
}

void sprite_batch_set_color(Vec4 color)
{
    uint32_t r = (uint32_t)(color.x * 255.f);
    uint32_t g = (uint32_t)(color.y * 255.f);
    uint32_t b = (uint32_t)(color.z * 255.f);
    uint32_t a = (uint32_t)(color.w * 255.f);
    batch->color = (r << 24) | (g << 16) | (b << 8) | a;
}

void sprite_batch_set_depth(float depth)
{
    check(depth >= 0.0f && depth <= 1.0f);
    batch->depth = -depth;
}

void sprite_batch_set_flip(uint8_t flag)
{
    check(flag == SPRITE_FLIP_NONE || flag == SPRITE_FLIP_VERTICAL || flag == SPRITE_FLIP_HORIZONTAL || flag == SPRITE_FLIP_BOTH);
    batch->flip_flag = flag;
}

static inline uint32_t current_clip_id()
{
    return batch->clip_stack[array_size(batch->clip_stack) - 1];
}

void sprite_batch_push_clip_rect(Rect clip)
{
    if (array_size(batch->clip_stack) > 0)
    {
        clip = rect_intersection(batch->clip_rects[current_clip_id()], clip);
    }
    array_push(batch->clip_stack, (uint32_t)array_size(batch->clip_rects), batch->clip_rect_allocator);
    array_push(batch->clip_rects, clip, batch->clip_rect_allocator);
}

void sprite_batch_pop_clip_rect()
{
    if (array_size(batch->clip_stack) > 1) // Can't pop the root clip rect
    {
        array_pop(batch->clip_stack);
    }
}

static inline void push_sprite(const Texture *texture, Rect source_rect, Rect dest_rect, float rotation, Vec2 origin, uint32_t clip_id)
{
    const Vec2 inv_texture_size = texture ? make_vec2(1.f / texture->width, 1.f / texture->height) : make_vec2(1.f, 1.f);

    const Sprite_Info sprite = {
        .dest_position = make_vec2(dest_rect.x, dest_rect.y),
        .dest_size = make_vec2(dest_rect.w, dest_rect.h),
        .rotation = rotation,
        .origin = origin,
        .depth = batch->depth,
        .color = batch->color,
        .texture_id = texture ? texture->id : 0,
        .clip_id = clip_id,
        .flip_flag = batch->flip_flag,
        .source_position = make_vec2(source_rect.x * inv_texture_size.x, source_rect.y * inv_texture_size.y),
        .source_size = make_vec2(source_rect.w * inv_texture_size.x, source_rect.h * inv_texture_size.y),
    };

    array_push(batch->sprites, sprite, batch->sprite_allocator);
}

void sprite_batch_draw_sprite(Asset_Id texture_id, Rect source_rect, Vec2 position, float rotation, Vec2 scale, Vec2 origin)
{
    const Rect dest_rect = {
        .x = position.x,
        .y = position.y,
        .w = source_rect.w * scale.x,
        .h = source_rect.h * scale.y,
    };

    const uint32_t clip_id = current_clip_id();
    if (!rect_intersect(dest_rect, batch->clip_rects[clip_id]))
    {
        return;
    }

    const Texture *texture = asset_data(textures, texture_id);
    if (texture != 0 && texture->id == 0)
    {
        texture = 0;
    }

    push_sprite(texture, source_rect, dest_rect, rotation, origin, clip_id);
}

void sprite_batch_draw_sheet(Asset_Id sprite_sheet_id, int sprite_x, int sprite_y, Vec2 position, float rotation, Vec2 scale, Vec2 origin)
{
    const Sprite_Sheet *sprite_sheet = asset_data(sprite_sheets, sprite_sheet_id);
    if (sprite_sheet == 0)
    {
        return;
    }
    const Rect source_rect = {
        .x = (float)sprite_x * sprite_sheet->sprite_width,
        .y = (float)sprite_y * sprite_sheet->sprite_height,
        .w = (float)sprite_sheet->sprite_width,
        .h = (float)sprite_sheet->sprite_height,
    };
    sprite_batch_draw_sprite(sprite_sheet->texture_id, source_rect, position, rotation, scale, origin);
}

Rect sprite_batch_draw_text(Asset_Id font_id, const char *text, Vec2 position, float scale)
{
    const Sprite_Sheet *font = asset_data(sprite_sheets, font_id);
    if (font == 0)
    {
        return (Rect) { 0 };
    }

    const Texture *texture = asset_data(textures, font->texture_id);
    if (texture == 0 || texture->id == 0 || font->sprite_width <= 0)
    {
        return (Rect) { 0 };
    }

    const int num_cols = texture->width / font->sprite_width;
    if (num_cols <= 0)
    {
        return (Rect) { 0 };
    }

    const float glyph_w = font->sprite_width * scale;
    const float glyph_h = font->sprite_height * scale;

    const uint32_t clip_id = current_clip_id();
    const Rect clip = batch->clip_rects[clip_id];
    const bool line_visible = rect_intersect((Rect) { clip.x, position.y, clip.w, glyph_h }, clip);

    const float start_x = position.x;
    for (; *text; ++text)
    {
        const uint8_t c = *text;
        const Rect dest_rect = make_rect(position.x, position.y, glyph_w, glyph_h);
        // Space and control characters have no visible glyph
        if (line_visible && c > ' ' && rect_intersect(dest_rect, clip))
        {
            const int tile = c - ' ';
            const Rect source_rect = {
                .x = (float)((tile % num_cols) * font->sprite_width),
                .y = (float)((tile / num_cols) * font->sprite_height),
                .w = (float)font->sprite_width,
                .h = (float)font->sprite_height,
            };
            push_sprite(texture, source_rect, dest_rect, 0.f, (Vec2) { 0.f, 0.f }, clip_id);
        }
        position.x += glyph_w;
    }
    return (Rect) { start_x, position.y, position.x - start_x, glyph_h };
}

static const Vec2 corner_offsets[NUM_VERTICES_PER_SPRITE] = {
    { 0.f, 0.f },
    { 1.f, 0.f },
    { 0.f, 1.f },
    { 1.f, 1.f },
};

typedef struct Draw_Range {
    uint32_t start;
    uint32_t end;
    uint32_t num_textures;
    gfx_id textures[MAX_TEXTURE_SLOTS];
} Draw_Range;

static void render_chunk(const Sprite_Info *sprites, uint32_t count, gfx_id *bound_textures)
{
    check(count <= MAX_BATCH_SIZE);

    static Draw_Range ranges[MAX_BATCH_SIZE / (MAX_TEXTURE_SLOTS + 1) + 1];
    static uint8_t slots[MAX_BATCH_SIZE];
    uint32_t num_ranges = 1;
    ranges[0] = (Draw_Range) { 0 };

    for (uint32_t i = 0; i < count; ++i)
    {
        const gfx_id texture_id = sprites[i].texture_id;
        if (texture_id == 0)
        {
            slots[i] = UNTEXTURED_SLOT;
            continue;
        }

        Draw_Range *range = &ranges[num_ranges - 1];
        uint32_t slot = 0;
        while (slot < range->num_textures && range->textures[slot] != texture_id)
        {
            ++slot;
        }
        if (slot == range->num_textures)
        {
            if (range->num_textures == MAX_TEXTURE_SLOTS)
            {
                // Out of slots: close this range and start a new one here
                range->end = i;
                range = &ranges[num_ranges++];
                *range = (Draw_Range) { .start = i };
                slot = 0;
            }
            range->textures[range->num_textures++] = texture_id;
        }
        slots[i] = (uint8_t)slot;
    }
    ranges[num_ranges - 1].end = count;

    const GLsizeiptr upload_size = sizeof(Sprite_Vertex) * NUM_VERTICES_PER_SPRITE * count;
    void *mapped_buffer = glMapBufferRange(GL_ARRAY_BUFFER, 0, upload_size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    fatal_check(mapped_buffer != 0);

    Sprite_Vertex *vertices = (Sprite_Vertex *)mapped_buffer;
    for (uint32_t i = 0; i < count; ++i)
    {
        const Rect clip_rect = batch->clip_rects[sprites[i].clip_id];
        const Sprite_Info *sprite = &sprites[i];
        
        Mat22 rotation = *mat22_identity();
        if (sprite->rotation != 0)
        {
            float sin_angle = sinf(sprite->rotation);
            float cos_angle = cosf(sprite->rotation);
            rotation.xx = cos_angle;
            rotation.xy = -sin_angle;
            rotation.yx = sin_angle;
            rotation.yy = cos_angle;
        }
        for (uint64_t v_idx = 0; v_idx < NUM_VERTICES_PER_SPRITE; ++v_idx)
        {
            const Vec2 corner_offset = vec2_element_mul(corner_offsets[v_idx], sprite->dest_size);
            const Vec2 local_pos = vec2_sub(corner_offset, sprite->origin);
            const Vec2 position = vec2_add(mat22_transform(rotation, local_pos), sprite->dest_position);

            vertices[v_idx].x = position.x;
            vertices[v_idx].y = position.y;
            vertices[v_idx].depth = sprite->depth;
            vertices[v_idx].color = sprite->color;
            vertices[v_idx].clip = clip_rect;
            vertices[v_idx].uv = vec2_add(vec2_element_mul(corner_offsets[v_idx ^ sprite->flip_flag], sprite->source_size), sprite->source_position);
            vertices[v_idx].texture_slot = slots[v_idx];
        }

        vertices += NUM_VERTICES_PER_SPRITE;
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    for (uint32_t r = 0; r < num_ranges; ++r)
    {
        const Draw_Range *range = &ranges[r];
        for (uint32_t slot = 0; slot < range->num_textures; ++slot)
        {
            if (bound_textures[slot] != range->textures[slot])
            {
                glActiveTexture(GL_TEXTURE0 + slot);
                glBindTexture(GL_TEXTURE_2D, range->textures[slot]);
                bound_textures[slot] = range->textures[slot];
            }
        }

        const uint64_t index_offset = (uint64_t)range->start * NUM_INDICES_PER_SPRITE;
        const uint64_t index_count = (uint64_t)(range->end - range->start) * NUM_INDICES_PER_SPRITE;
        glDrawElements(GL_TRIANGLES, (GLsizei)index_count, GL_UNSIGNED_SHORT, (const void *)(index_offset * sizeof(uint16_t)));
    }
}

void sprite_batch_begin_frame(Rect viewport)
{
    if (!batch->initialized)
    {
        return;
    }

    if (batch->last_viewport.x != viewport.x || batch->last_viewport.y != viewport.y ||
        batch->last_viewport.w != viewport.w || batch->last_viewport.h != viewport.h)
    {
        orthographic_projection_from_frustum(&batch->projection_matrix, viewport.x, viewport.w, viewport.y, viewport.h, -1.f, 1.f);
        batch->last_viewport = viewport;
    }

    check(array_size(batch->clip_stack) == 0);
    sprite_batch_push_clip_rect(viewport);
}

void sprite_batch_end_frame()
{
    if (!batch->initialized)
    {
        return;
    }

    const Vec4 color_value = (Vec4) { 0.1f, 0.1f, 0.1f, 1.f };
    const float depth_value = 0.f;
    glClearBufferfv(GL_COLOR, 0, &color_value.x);
    glClearBufferfv(GL_DEPTH, 0, &depth_value);

    glUseProgram(batch->shader);

    glUniformMatrix4fv(batch->transform_location, 1, 0, (const float *)&batch->projection_matrix);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->index_buffer);

    {
        const GLsizei stride = sizeof(Sprite_Vertex);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)offsetof(Sprite_Vertex, x));

        glEnableVertexAttribArray(1);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, (const GLvoid *)offsetof(Sprite_Vertex, color));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)offsetof(Sprite_Vertex, clip));

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)offsetof(Sprite_Vertex, uv));

        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, stride, (const GLvoid *)offsetof(Sprite_Vertex, texture_slot));
    }

    gfx_id bound_textures[MAX_TEXTURE_SLOTS] = { 0 };

    const uint64_t num_sprites = array_size(batch->sprites);
    for (uint64_t chunk_start = 0; chunk_start < num_sprites; chunk_start += MAX_BATCH_SIZE)
    {
        const uint64_t remaining = num_sprites - chunk_start;
        render_chunk(&batch->sprites[chunk_start], (uint32_t)(remaining < MAX_BATCH_SIZE ? remaining : MAX_BATCH_SIZE), bound_textures);
    }

    // Reset state
    array_reset(batch->sprites);
    array_reset(batch->clip_rects);
    array_reset(batch->clip_stack);
    batch->color = 0xffffffff;
    batch->depth = 0.f;
    batch->flip_flag = SPRITE_FLIP_NONE;
}