#pragma once
#include "basic.h"

// Replacement for `sprintf()`
int c_print_unsafe(char *buf, const char *fmt, ...);

// Replacement for `snprintf()`
int c_print(char *buf, int count, const char *fmt, ...);

// Replacement for `vsprintf()`
int c_vprint_unsafe(char *buf, const char *fmt, va_list va);

// Replacement for `vsnprintf()`
int c_vprint(char *buf, int count, const char *fmt, va_list va);
