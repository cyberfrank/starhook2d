#pragma once
#include "basic.h"

struct Allocator;

// 32->8
String8 utf32_to_utf8(const uint32_t *utf32, struct Allocator *a);
String8 utf32_to_utf8_n(const uint32_t *utf32, uint32_t count, struct Allocator *a);

uint32_t utf8_num_codepoints(const char *utf8);
uint32_t utf8_num_codepoints_n(const char *utf8, uint32_t n);

// 8->32
String32 utf8_to_utf32(const char *utf8, struct Allocator *a);
String32 utf8_to_utf32_n(const char *utf8, uint32_t n, struct Allocator *a);

// 16->8
String8 utf16_to_utf8(const uint16_t *utf16, struct Allocator *a);
String8 utf16_to_utf8_n(const uint16_t *utf16, uint32_t n, struct Allocator *a);

// 8->16
String16 utf8_to_utf16(const char *utf8, struct Allocator *a);
String16 utf8_to_utf16_n(const char *utf8, uint32_t n, struct Allocator *a);

// 8 decode/encode
uint32_t utf8_decode(const char **utf8_in);
uint32_t utf8_decode_n(uint32_t *codepoints, uint32_t n, const char *utf8);
char *utf8_encode(char *utf8_in, uint32_t codepoint);

// 16 encode/decode
uint32_t utf16_decode(const uint16_t **utf16);
uint16_t *utf16_encode(uint16_t *utf16, uint32_t codepoint);

static inline bool inside_multibyte_codepoint(uint8_t c)
{
    return c >= 0x80 && c < 0xc0;
}

static inline uint32_t num_bytes_in_codepoint(uint32_t c)
{
    if (c < 0x80U)
        return 1;
    else if (c < 0x800U)
        return 2;
    else if (c < 0x10000U)
        return 3;
    else if (c < 0x110000U)
        return 4;
    else
        return 0;
}

// Convert caret position from 8-bit granularity to 32-bit
// Count how many multibyte codepoints the original caret position passes
static inline uint32_t utf8_caret_to_utf32(uint8_t *data, uint32_t caret8)
{
    uint32_t num_multibyte_codepoints = 0;
    for (uint32_t i = 0; i < caret8; ++i) {
        if (inside_multibyte_codepoint(data[i]))
            ++num_multibyte_codepoints;
    }
    return (caret8 - num_multibyte_codepoints);
}

// Convert caret position from 32-bit to 8-bit
static inline uint32_t utf32_caret_to_utf8(uint32_t *data, uint32_t caret32)
{
    uint32_t new_caret_pos = 0;
    for (uint32_t i = 0; i < caret32; ++i)
        new_caret_pos += num_bytes_in_codepoint(data[i]);
    
    return new_caret_pos;
}