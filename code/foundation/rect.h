#pragma once
#include "basic.h"

// Tests
static inline bool point_in_rect(Vec2 p, Rect r);
static inline bool rect_intersect(Rect r1, Rect r2);

// Edges and corners
static inline float rect_right(Rect r);
static inline float rect_bottom(Rect r);

static inline Vec2 rect_interpolate_point(Rect r, float x_frac, float y_frac);
static inline Vec2 rect_center(Rect r);
static inline Rect rect_center_in(float w, float h, Rect big_r);

// Insets
static inline Rect rect_inset(Rect r, float x, float y);

// Splitting
typedef union Rect_Split {
    struct {
        Rect left;
        Rect right;
    };
    struct {
        Rect top;
        Rect bottom;
    };
} Rect_Split;

static inline Rect_Split rect_split_left(Rect r, float w, float margin);
static inline Rect_Split rect_split_right(Rect r, float w, float margin);
static inline Rect_Split rect_split_top(Rect r, float h, float margin);
static inline Rect_Split rect_split_bottom(Rect r, float h, float margin);

// Dividing
static inline Rect rect_divide_x(Rect r, float margin, uint32_t n, uint32_t idx);
static inline Rect rect_divide_y(Rect r, float margin, uint32_t n, uint32_t idx);

// Intersection and union
static inline Rect rect_intersection(Rect r1, Rect r2);
static inline Rect rect_union(Rect r1, Rect r2);


static inline bool point_in_rect(Vec2 p, Rect r)
{
    return p.x > r.x && p.x <= r.x + r.w && p.y > r.y && p.y <= r.y + r.h;
}

static inline bool rect_intersect(Rect r1, Rect r2)
{
    const float left = c_max(r1.x, r2.x);
    const float top = c_max(r1.y, r2.y);
    const float right = c_min(rect_right(r1), rect_right(r2));
    const float bottom = c_min(rect_bottom(r1), rect_bottom(r2));
    return left <= right && top <= bottom;
}

static inline float rect_right(Rect r)
{
    return r.x + r.w;
}

static inline float rect_bottom(Rect r)
{
    return r.y + r.h;
}

static inline Vec2 rect_interpolate_point(Rect r, float x_frac, float y_frac)
{
    return (Vec2) { r.x + r.w * x_frac, r.y + r.h * y_frac };
}

static inline Vec2 rect_center(Rect r)
{
    return rect_interpolate_point(r, 0.5f, 0.5f);
}

static inline Rect rect_center_in(float w, float h, Rect big_r)
{
    const Vec2 c = rect_center(big_r);
    return (Rect) { c.x - w / 2, c.y - h / 2, w, h };
}

static inline Rect rect_inset(Rect r, float x, float y)
{
    return (Rect) { r.x + x, r.y + y, r.w - x * 2, r.h - y * 2 };
}

static inline Rect_Split rect_split_left(Rect r, float w, float margin)
{
    return (const Rect_Split) {
        .left = { r.x, r.y, w, r.h },
        .right = { r.x + w + margin, r.y, r.w - w - margin, r.h },
    };
}

static inline Rect_Split rect_split_right(Rect r, float w, float margin)
{
    return (const Rect_Split) {
        .left = { r.x, r.y, r.w - w - margin, r.h },
        .right = { r.x + r.w - w, r.y, w, r.h },
    };
}

static inline Rect_Split rect_split_top(Rect r, float h, float margin)
{
    return (const Rect_Split) {
        .top = { r.x, r.y, r.w, h },
        .bottom = { r.x, r.y + h + margin, r.w, r.h - h - margin },
    };
}

static inline Rect_Split rect_split_bottom(Rect r, float h, float margin)
{
    return (const Rect_Split) {
        .top = { r.x, r.y, r.w, r.h - h - margin },
        .bottom = { r.x, r.y + r.h - h, r.w, h },
    };
}

static inline Rect rect_divide_x(Rect r, float margin, uint32_t n, uint32_t idx)
{
    const float ww = (r.w - margin * (n - 1)) / n;
    return (Rect) { r.x + (ww + margin) * idx, r.y, ww, r.h };
}

static inline Rect rect_divide_y(Rect r, float margin, uint32_t n, uint32_t idx)
{
    const float hh = (r.h - margin * (n - 1)) / n;
    return (Rect) { r.x, r.y + (hh + margin) * idx, r.w, hh };
}

static inline Rect rect_intersection(Rect r1, Rect r2)
{
    const float left = c_max(r1.x, r2.x);
    const float top = c_max(r1.y, r2.y);
    const float right = c_min(rect_right(r1), rect_right(r2));
    const float bottom = c_min(rect_bottom(r1), rect_bottom(r2));
    const Rect zero = { 0 };
    const Rect intersection = { left, top, right - left, bottom - top };
    return left > right || top > bottom ? zero : intersection;
}

static inline Rect rect_union(Rect r1, Rect r2)
{
    const float left = c_min(r1.x, r2.x);
    const float top = c_min(r1.y, r2.y);
    const float right = c_max(rect_right(r1), rect_right(r2));
    const float bottom = c_max(rect_bottom(r1), rect_bottom(r2));
    return (Rect) { left, top, right - left, bottom - top };
}