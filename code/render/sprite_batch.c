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
    layout (location = 0) in vec4 sprite_data;\
    layout (location = 1) in vec4 clip;\
    layout (location = 2) in vec2 uv;\
    uniform mat4 transform;\
    out vec2 out_position;\
    out vec4 out_color;\
    out vec4 out_clip;\
    out vec2 out_uv;\
    vec4 decode_rgba(uint c) {\
        return vec4((c >> 24) & 0xff, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff) / 255.f;\
    }\
    void main() {\
        out_position = sprite_data.xy;\
        out_uv = uv;\
        out_color = decode_rgba(floatBitsToUint(sprite_data.w));\
        out_clip = clip;\
        out_clip.zw += out_clip.xy;\
        gl_Position = transform * vec4(sprite_data.xyz, 1.0);\
    }";

const char *sprite_fs = "\
    #version 430 core\n\
    in vec2 out_position;\
    in vec4 out_color;\
    in vec4 out_clip;\
    in vec2 out_uv;\
    out vec4 FragColor;\
    layout (binding = 0) uniform sampler2D tex;\
    void main() {\
        if (out_position.x < out_clip.x || out_position.x > out_clip.z || out_position.y < out_clip.y || out_position.y > out_clip.w)\
            discard;\
        vec4 tex_color = texture(tex, out_uv) * out_color;\
        if (tex_color.a < 0.1f)\
            discard;\
        FragColor = tex_color;\
    }";

enum {
    MAX_BATCH_SIZE = 2048,
    MIN_BATCH_SIZE = 128,
    NUM_VERTICES_PER_SPRITE = 4,
    NUM_INDICES_PER_SPRITE = 6,
};

typedef struct Sprite_Vertex {
    Vec4 position;
    Rect clip;
    Vec2 uv;
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
    uint64_t vertex_buffer_offset;
    Rect *clip_rects;

    Texture default_texture;
    Mat44 projection_matrix;
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
    batch->vertex_buffer_offset = 0;
    batch->sprite_allocator = system_allocator;
    batch->clip_rect_allocator = system_allocator;
    batch->color = 0xffffffff;
    batch->depth = 0.f;
    batch->flip_flag = SPRITE_FLIP_NONE;

    const uint8_t white_pixel[4] = { 0xff, 0xff, 0xff, 0xff };
    batch->default_texture = (Texture) {
        .width = 1,
        .height = 1,
        .id = create_texture_2d(1, 1, &white_pixel),
    };

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

void sprite_batch_push_clip_rect(Rect clip)
{
    if (array_size(batch->clip_rects) > 0)
    {
        const Rect parent_clip = batch->clip_rects[array_size(batch->clip_rects) - 1];
        array_push(batch->clip_rects, rect_intersection(parent_clip, clip), batch->clip_rect_allocator);
    }
    else
    {
        array_push(batch->clip_rects, clip, batch->clip_rect_allocator);
    }
}

void sprite_batch_pop_clip_rect()
{
    if (array_size(batch->clip_rects) > 1) // Can't pop the root clip rect
    {
        array_pop(batch->clip_rects);
    }
}

void sprite_batch_draw_sprite(Asset_Id texture_id, Rect source_rect, Vec2 position, float rotation, Vec2 scale, Vec2 origin)
{
    Rect dest_rect = {
        .x = position.x,
        .y = position.y,
        .w = source_rect.w * scale.x,
        .h = source_rect.h * scale.y,
    };

    uint32_t clip_id = (uint32_t)array_size(batch->clip_rects) - 1;
    if (!rect_intersect(dest_rect, batch->clip_rects[clip_id]))
    {
        return;
    }

    Texture *texture = asset_data(textures, texture_id);
    if (texture == 0 || texture->id == 0)
    {
        texture = &batch->default_texture;
    }

    const Vec2 inv_texture_size = (Vec2) { 1.f / texture->width, 1.f / texture->height };
    const Vec2 src_position_pixels = (Vec2) { source_rect.x, source_rect.y };
    const Vec2 src_size_pixels = (Vec2) { source_rect.w, source_rect.h };

    const Sprite_Info sprite = {
        .dest_position = (Vec2) { dest_rect.x, dest_rect.y },
        .dest_size = (Vec2) { dest_rect.w, dest_rect.h },
        .rotation = rotation,
        .origin = origin,
        .depth = batch->depth,
        .color = batch->color,
        .texture_id = texture->id,
        .clip_id = clip_id,
        .flip_flag = batch->flip_flag,
        .source_position = vec2_element_mul(src_position_pixels, inv_texture_size),
        .source_size = vec2_element_mul(src_size_pixels, inv_texture_size),
    };

    array_push(batch->sprites, sprite, batch->sprite_allocator);
}

void sprite_batch_draw_sheet(Asset_Id sprite_sheet_id, int sprite_x, int sprite_y, Vec2 position, float rotation, Vec2 scale, Vec2 origin)
{
    Sprite_Sheet *sprite_sheet = asset_data(sprite_sheets, sprite_sheet_id);
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
    Sprite_Sheet *font = asset_data(sprite_sheets, font_id);
    if (font == 0)
    {
        return (Rect) { 0 };
    }
    
    Texture *texture = asset_data(textures, font->texture_id);
    if (texture == 0 || texture->id == 0)
    {
        return (Rect) { 0 };
    }

    const int num_cols = texture->width / font->sprite_width;
    float start_x = position.x;
    while (*text)
    {
        const uint8_t c = *text;
        const int tile_x = (c - ' ') % num_cols;
        const int tile_y = (c - ' ') / num_cols;
        sprite_batch_draw_sheet(font_id, tile_x, tile_y, position, 0.0f, (Vec2) { scale, scale }, (Vec2) { 0.0f, 0.0f });
        position.x += font->sprite_width * scale;
        ++text;
    }
    return (Rect) { start_x, position.y, position.x - start_x, (float)font->sprite_height * scale };
}

static const Vec2 corner_offsets[NUM_VERTICES_PER_SPRITE] = {
    { 0.f, 0.f },
    { 1.f, 0.f },
    { 0.f, 1.f },
    { 1.f, 1.f },
};

static void render_sprite(const Sprite_Info *sprite, const Rect *clip_rect, Sprite_Vertex *vertices)
{
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

    for (uint64_t i = 0; i < NUM_VERTICES_PER_SPRITE; ++i)
    {
        const Vec2 corner_offset = vec2_element_mul(corner_offsets[i], sprite->dest_size);
        const Vec2 local_pos = vec2_sub(corner_offset, sprite->origin);
        const Vec2 position = vec2_add(mat22_transform(rotation, local_pos), sprite->dest_position);
        
        vertices[i].position = (Vec4) { position.x, position.y, sprite->depth, *(float *)&sprite->color };
        vertices[i].clip = *clip_rect;
        vertices[i].uv = vec2_add(vec2_element_mul(corner_offsets[i ^ sprite->flip_flag], sprite->source_size), sprite->source_position);
    }
}

static void render_batch(gfx_id texture_id, const Sprite_Info *sprites, uint64_t count)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    while (count > 0)
    {
        uint64_t batch_size = count;

        const uint64_t space_left = MAX_BATCH_SIZE - batch->vertex_buffer_offset;
        if (batch_size > space_left)
        {
            if (space_left < MIN_BATCH_SIZE)
            {
                // Buffer almost out of space so we start over
                batch->vertex_buffer_offset = 0;
                batch_size = count < MAX_BATCH_SIZE ? count : MAX_BATCH_SIZE;
            }
            else
            {
                batch_size = space_left;
            }
        }

        // Copy sprites into buffer
        void *mapped_buffer = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        {
            fatal_check(mapped_buffer != 0);
            Sprite_Vertex *vertices = (Sprite_Vertex *)mapped_buffer + batch->vertex_buffer_offset * NUM_VERTICES_PER_SPRITE;
            for (uint64_t sprite_idx = 0; sprite_idx < batch_size; ++sprite_idx)
            {
                check(sprite_idx < count);
                const Rect clip_rect = batch->clip_rects[sprites[sprite_idx].clip_id];
                render_sprite(&sprites[sprite_idx], &clip_rect, vertices);
                vertices += NUM_VERTICES_PER_SPRITE;
            }
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);

        uint64_t index_offset = batch->vertex_buffer_offset * NUM_INDICES_PER_SPRITE;
        uint64_t index_count = batch_size * NUM_INDICES_PER_SPRITE;
        glDrawElements(GL_TRIANGLES, (GLsizei)index_count, GL_UNSIGNED_SHORT, (const void *)(index_offset * sizeof(uint16_t)));

        sprites += batch_size;
        count -= batch_size;

        batch->vertex_buffer_offset += batch_size;
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

    check(array_size(batch->clip_rects) == 0);
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

    const void *transform = &batch->projection_matrix;
    const int loc = glGetUniformLocation(batch->shader, "transform");
    glUniformMatrix4fv(loc, 1, 0, transform);

    glBindBuffer(GL_ARRAY_BUFFER, batch->vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->index_buffer);

    {
        uint32_t offset = 0;

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite_Vertex), (const GLvoid *)(GLintptr)offset);
        offset += sizeof(float) * 4;

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Sprite_Vertex), (const GLvoid *)(GLintptr)offset);
        offset += sizeof(float) * 4;

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Sprite_Vertex), (const GLvoid *)(GLintptr)offset);
        offset += sizeof(float) * 2;
    }

    gfx_id batch_texture_id = 0;
    uint64_t batch_start = 0;

    for (uint64_t pos = 0; pos < array_size(batch->sprites); ++pos)
    {
        gfx_id texture_id = batch->sprites[pos].texture_id;
        check(texture_id != 0);

        if (texture_id != batch_texture_id)
        {
            if (pos > batch_start)
            {
                render_batch(batch_texture_id, &batch->sprites[batch_start], pos - batch_start);
            }
            batch_texture_id = texture_id;
            batch_start = pos;
        }
    }
    render_batch(batch_texture_id, &batch->sprites[batch_start], array_size(batch->sprites) - batch_start);

    // Reset state
    array_reset(batch->sprites);
    array_reset(batch->clip_rects);
    batch->color = 0xffffffff;
    batch->depth = 0.f;
    batch->flip_flag = SPRITE_FLIP_NONE;
}