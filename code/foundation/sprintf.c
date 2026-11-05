#include "sprintf.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

int c_print_unsafe(char *buf, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int res = stbsp_vsprintf(buf, fmt, va);
    va_end(va);
    return res;
}

int c_print(char *buf, int count, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int res = stbsp_vsnprintf(buf, count, fmt, va);
    va_end(va);
    return res;
}

int c_vprint_unsafe(char *buf, const char *fmt, va_list va)
{
    return stbsp_vsprintf(buf, fmt, va);
}

int c_vprint(char *buf, int count, const char *fmt, va_list va)
{
    return stbsp_vsnprintf(buf, count, fmt, va);
}
