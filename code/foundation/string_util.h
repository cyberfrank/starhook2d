#pragma once
#include "basic.h"

#include <string.h>
#include <ctype.h>

static inline const char *get_file_extension(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (ext)
    {
        return ++ext;
    }
    return  0;
}

static inline const char *get_file_name(const char *path)
{
    const char *name = strrchr(path, '/');
    if (name)
    {
        return ++name;
    }
    return 0;
}

static inline bool is_non_word(char c) 
{
    if ((c >= 'A' && c <= 'Z') || 
        (c >= 'a' && c <= 'z') || 
        (c >= '0' && c <= '9') || 
        c == '_')
    {
        return false;
    }
    if (c >= 0x80 && c < 0xc0)
    {
        // Multi-byte codepoint
        return false;
    }
    return true;
}

// Check if a string has the starts with the given preamble.
static inline bool string_starts_with(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

static inline void strncpy_safe(char *dst, const char *src, uint64_t len)
{
#if defined(_MSC_VER)
    const uint32_t *res = (uint32_t *)_memccpy(dst, src, 0, len);
#else
    const uint32_t *res = (uint32_t *)memccpy(dst, src, 0, len);
#endif
    if (!res)
        *(dst + len) = 0;
}
