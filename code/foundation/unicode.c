#include "unicode.h"
#include "allocator.h"

static inline uint32_t private__codepoint_len_in_utf8(uint32_t codepoint)
{
    const uint32_t c = codepoint;
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

static uint32_t private__utf32_to_utf8_len(const uint32_t *utf32)
{
    uint32_t len = 0;
    while (*utf32)
        len += private__codepoint_len_in_utf8(*utf32++);
    return len;
}

static uint32_t private__utf32_to_utf8_len_n(const uint32_t *utf32, uint32_t count)
{
    uint32_t len = 0;
    const uint32_t *end = utf32 + count;
    while (utf32 < end)
        len += private__codepoint_len_in_utf8(*utf32++);
    return len;
}

String8 utf32_to_utf8(const uint32_t *utf32, struct Allocator *a)
{
    const uint32_t len = private__utf32_to_utf8_len(utf32);
    char *s = c_alloc(a, len + 1);
    char *start = s;
    while (*utf32)
        s = utf8_encode(s, *utf32++);
    *s++ = 0;
    return (String8) { .str = start, .len = len };
}

static inline uint32_t private__utf8_byte_sequence_len(uint8_t c)
{
    if ((c & 0x80) == 0x0)
        return 1;
    else if ((c & 0xe0) == 0xc0)
        return 2;
    else if ((c & 0xf0) == 0xe0)
        return 3;
    else if ((c & 0xf8) == 0xf0)
        return 4;
    else
        return 1;
}

static inline char *private__utf8_advance(const char *utf8)
{
    return (char *)(utf8 + private__utf8_byte_sequence_len(*utf8));
}

uint32_t utf8_num_codepoints(const char *utf8)
{
    uint32_t len = 0;
    while (*utf8) {
        ++len;
        utf8 = private__utf8_advance(utf8);
    }
    return len;
}

uint32_t utf8_num_codepoints_n(const char *utf8, uint32_t n)
{
    uint32_t len = 0;
    const char *end = utf8 + n;
    while (utf8 < end) {
        ++len;
        utf8 = private__utf8_advance(utf8);
    }
    return len;
}

String32 utf8_to_utf32(const char *utf8, struct Allocator *a)
{
    const uint32_t len = utf8_num_codepoints(utf8);
    uint32_t *ws = c_alloc(a, (len + 1) * sizeof(uint32_t));
    uint32_t *ws_start = ws;
    while (*utf8)
        *ws++ = utf8_decode(&utf8);
    *ws++ = 0;
    return (String32) { .str = ws_start, .len = len };
}

String32 utf8_to_utf32_n(const char *utf8, uint32_t n, Allocator *a)
{
    const uint32_t len = utf8_num_codepoints_n(utf8, n);
    uint32_t *ws = c_alloc(a, (len + 1) * sizeof(uint32_t));
    uint32_t *ws_start = ws;
    const char *end = utf8 + n;
    while (utf8 < end)
        *ws++ = utf8_decode(&utf8);
    *ws++ = 0;
    return (String32) { .str = ws_start, .len = len };
}

String8 utf32_to_utf8_n(const uint32_t *utf32, uint32_t size, struct Allocator *a)
{
    const uint32_t len = private__utf32_to_utf8_len_n(utf32, size);
    char *s = c_alloc(a, len + 1);
    char *start = s;
    const uint32_t *end = utf32 + size;
    while (utf32 < end)
        s = utf8_encode(s, *utf32++);
    *s++ = 0;
    return (String8) { .str = start, .len = len };
}

static uint32_t private__utf16_to_utf8_len(const uint16_t *utf16)
{
    uint32_t len = 0;
    while (*utf16)
        len += private__codepoint_len_in_utf8(utf16_decode(&utf16));
    return len;
}

static uint32_t private__utf16_to_utf8_len_n(const uint16_t *utf16, uint32_t count)
{
    uint32_t len = 0;
    const uint16_t *end = utf16 + count;
    while (utf16 < end)
        len += private__codepoint_len_in_utf8(utf16_decode(&utf16));
    return len;
}

String8 utf16_to_utf8(const uint16_t *utf16, struct Allocator *a)
{
    const uint32_t len = private__utf16_to_utf8_len(utf16);
    char *s = c_alloc(a, len + 1);
    char *start = s;
    while (*utf16)
        s = utf8_encode(s, utf16_decode(&utf16));
    *s++ = 0;
    return (String8) { .str = start, .len = len };
}

String8 utf16_to_utf8_n(const uint16_t *utf16, uint32_t size, struct Allocator *a)
{
    uint32_t len = private__utf16_to_utf8_len_n(utf16, size);
    char *s = c_alloc(a, len + 1);
    char *start = s;
    const uint16_t *end = utf16 + size;
    while (utf16 < end)
        s = utf8_encode(s, utf16_decode(&utf16));
    *s++ = 0;
    return (String8) { .str = start, .len = len };
}

static uint32_t private__utf16_len(const char *utf8)
{
    uint32_t len = 0;
    while (*utf8) {
        const uint32_t codepoint = utf8_decode(&utf8);
        ++len;
        if (codepoint >= 0x10000U)
            ++len;
    }
    return len;
}

static uint32_t private__utf16_len_n(const char *utf8, uint32_t n)
{
    uint32_t len = 0;
    const char *end = utf8 + n;
    while (utf8 < end) 
    {
        const uint32_t codepoint = utf8_decode(&utf8);
        ++len;
        if (codepoint >= 0x10000U)
            ++len;
    }
    return len;
}

String16 utf8_to_utf16(const char *utf8, Allocator *allocator)
{
    const uint32_t len = private__utf16_len(utf8);
    uint16_t *ws = c_alloc(allocator, (len + 1) * sizeof(uint16_t));
    uint16_t *ws_start = ws;
    while (*utf8)
        ws = utf16_encode(ws, utf8_decode(&utf8));
    *ws++ = 0;
    return (String16) { .str = ws_start, .len = len };
}

String16 utf8_to_utf16_n(const char *utf8, uint32_t n, struct Allocator *a)
{
    const uint32_t len = private__utf16_len_n(utf8, n);
    uint16_t *ws = c_alloc(a, (len + 1) * sizeof(uint16_t));
    uint16_t *ws_start = ws;
    const char *end = utf8 + n;
    while (utf8 < end)
        ws = utf16_encode(ws, utf8_decode(&utf8));
    *ws++ = 0;
    return (String16) { .str = ws_start, .len = len };
}

uint32_t utf8_decode(const char **utf8_in)
{
    const uint8_t **utf8 = (const uint8_t **)utf8_in;
    const uint32_t c = *(*utf8)++;
    if ((c & 0x80) == 0) {
        return c;
    } 
    else if ((c & 0xe0) == 0xc0) {
        uint32_t d = *(*utf8)++;
        return ((c & 0x1f) << 6) | (d & 0x3f);
    } 
    else if ((c & 0xf0) == 0xe0) {
        uint32_t d0 = *(*utf8)++;
        uint32_t d1 = *(*utf8)++;
        return ((c & 0x0f) << 12) | ((d0 & 0x3f) << 6) | (d1 & 0x3f);
    } 
    else if ((c & 0xf8) == 0xf0) {
        uint32_t d0 = *(*utf8)++;
        uint32_t d1 = *(*utf8)++;
        uint32_t d2 = *(*utf8)++;
        return ((c & 0x07) << 18) | ((d0 & 0x3f) << 12) | ((d1 & 0x3f) << 6) | (d2 & 0x3f);
    } 
    else {
        return 0;
    }
}

uint32_t utf8_decode_n(uint32_t *codepoints, uint32_t max_codepoints, const char *utf8)
{
    const char *s = utf8;
    uint32_t len = 0;
    while (*s && len < max_codepoints)
        codepoints[len++] = utf8_decode(&s);
    return len;
}

char *utf8_encode(char *utf8_in, uint32_t codepoint)
{
    char *utf8 = utf8_in;
    const uint32_t c = codepoint;
    if (c < 0x80U) {
        *utf8 = (uint8_t)c;
        return utf8 + 1;
    }
    else if (c < 0x800U) {
        utf8[0] = (uint8_t)((c >> 6) | 0xc0);
        utf8[1] = (uint8_t)((c & 0x3f) | 0x80);
        return utf8 + 2;
    }
    else if (c < 0x10000U) {
        utf8[0] = (uint8_t)((c >> 12) | 0xe0);
        utf8[1] = (uint8_t)(((c >> 6) & 0x3f) | 0x80);
        utf8[2] = (uint8_t)((c & 0x3f) | 0x80);
        return utf8 + 3;
    }
    else if (c < 0x110000U) {
        utf8[0] = (uint8_t)((c >> 18) | 0xf0);
        utf8[1] = (uint8_t)(((c >> 12) & 0x3F) | 0x80);
        utf8[2] = (uint8_t)(((c >> 6) & 0x3F) | 0x80);
        utf8[3] = (uint8_t)((c & 0x3F) | 0x80);
        return utf8 + 4;
    } else {
        return utf8;
    }
}

uint32_t utf16_decode(const uint16_t **utf16)
{
    uint32_t c = *(*utf16)++;
    if (c < 0xd800U || c > 0xdfffU)
    {
        return c;
    }
    else 
    {
        uint32_t high_surrogate = c - 0xd800U;
        uint32_t low_surrogate = *(*utf16)++ - 0xdc00U;
        uint32_t offset = (high_surrogate << 10) + low_surrogate;
        return offset + 0x10000U;
    }
}

uint16_t *utf16_encode(uint16_t *utf16, uint32_t codepoint)
{
    if (codepoint >= 0xd800U && codepoint <= 0xdfff)
    {
        // Cannot encode surrogate pair character as UTF-16
    }
    else if (codepoint >= 0x10000U) 
    {
        uint32_t offset = codepoint - 0x10000U;
        uint32_t high_surrogate = 0xd800U + (offset >> 10);
        uint32_t low_surrogate = 0xdc00U + (offset & 0x3ffU);
        *utf16++ = (uint16_t)high_surrogate;
        *utf16++ = (uint16_t)low_surrogate;
    } 
    else
    {
        *utf16++ = (uint16_t)codepoint;
    }
    return utf16;
}
