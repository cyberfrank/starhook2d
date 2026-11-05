#pragma once
#include "foundation/basic.h"

enum {
    SPRITE_FLIP_NONE = 0,
    SPRITE_FLIP_VERTICAL = 1,
    SPRITE_FLIP_HORIZONTAL = 2,
    SPRITE_FLIP_BOTH = SPRITE_FLIP_VERTICAL | SPRITE_FLIP_HORIZONTAL,
};

void sprite_batch_create();
void sprite_batch_destroy();

void sprite_batch_set_color(Vec4 color);
void sprite_batch_set_depth(float depth);
void sprite_batch_set_flip(uint8_t sprite_flip);

void sprite_batch_push_clip_rect(Rect clip);
void sprite_batch_pop_clip_rect();

void sprite_batch_draw_sprite(Asset_Id texture_id, Rect source_rect, Vec2 position, float rotation, Vec2 scale, Vec2 origin);
void sprite_batch_draw_sheet(Asset_Id sprite_sheet_id, int sprite_x, int sprite_y, Vec2 position, float rotation, Vec2 scale, Vec2 origin);
Rect sprite_batch_draw_text(Asset_Id font_id, const char *text, Vec2 position, float scale);

void sprite_batch_begin_frame(Rect viewport);
void sprite_batch_end_frame();