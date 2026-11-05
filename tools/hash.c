#pragma once
#include "foundation/basic.h"
#include "foundation/murmur_hash64.h"
#include "foundation/allocator.h"
#include "foundation/os.h"
#include <stdio.h>

int main(int argc, char **argv) {
    const char *file = argv[1];
    if (file == 0) 
    {
        printf("No file path provided!");
        return 1;
    }

    uint32_t num_patched = 0;
    uint32_t num_occurences = 0;

    uint64_t sz;
    uint64_t o = 0;
    char *text = os_read_entire_file(file, &sz, system_allocator);
    char buffer[1024] = { 0 };

    const char *MACRO_STR = "STATIC_HASH";
    const uint32_t MACRO_LEN = 12;

    while (o < sz) {
        if (text[o] == '\n') {
            ++o;
            continue;
        } else if (text[o] != 'S') {
            ++o;
            continue;
        }

        if (memcmp(text + o, MACRO_STR, MACRO_LEN - 1)) {
            ++o;
            continue;
        }

        ++num_occurences;

        const char *macro_s = text + o;
        const char *string_s = macro_s + MACRO_LEN + 1;
        const char *string_e = (char *)memchr(string_s, '"', sz - o - MACRO_LEN);
        const char *macro_e = (char *)memchr(string_e, ')', (uint64_t)(text + (int64_t)sz - string_e)) + 1;

        const uint32_t string_len = (uint32_t)(string_e - string_s);
        const uint32_t macro_len = (uint32_t)(macro_e - macro_s);

        const uint64_t hash = murmur_hash64a(string_s, string_len, 0);
        uint32_t buffer_len = snprintf(buffer, 1024, "%s(\"%.*s\", 0x%llxULL)", MACRO_STR, string_len, string_s, hash);
        if (buffer_len == macro_len && 0 == memcmp(macro_s, buffer, buffer_len)) {
            // Already patched
            o += buffer_len;
            continue;
        }

        printf("Patching %s\n", buffer);
        ++num_patched;

        const int64_t extra = (int64_t)buffer_len - (int64_t)macro_len;
        text = c_realloc(system_allocator, text, sz, (uint64_t)((int64_t)sz + extra));
        if (extra > 0)
            memmove(text + o + extra, text + o, sz - o);
        else
            memmove(text + o, text + o - extra, sz - o + extra);
        memcpy(text + o, buffer, buffer_len);
        sz = (uint64_t)((int64_t)sz + extra);
        o += buffer_len;
    }

    if (num_patched > 0)
    {
        os_write_to_file(file, text, sz);
    }

    printf("Patched %i/%i occurences.", num_patched, num_occurences);

    c_free(system_allocator, text, sz);
    return 0;
}