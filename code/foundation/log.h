#pragma once
#include "basic.h"
#include <string.h> // strrchr

enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_TRACE,
    LOG_FATAL,
};

typedef void (*log_func)(int level, const char *msg, uint32_t len);

void register_log_callback(log_func fn);
// Default stdout
void log_stdout(int level, const char *msg, uint32_t len);

void log_print(int level, const char *file, uint32_t line, const char *msg, ...);

#if defined(OS_WINDOWS)
    #define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
    #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define log_info(...)  log_print(LOG_INFO, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_print(LOG_WARN, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_error(...) log_print(LOG_ERROR, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_trace(...) log_print(LOG_TRACE, __FILENAME__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_print(LOG_FATAL, __FILENAME__, __LINE__, __VA_ARGS__)

#if !defined(NO_CHECKS)
    #define check(cond)             do { if(!(cond)) { log_error("Check failed: %s", #cond); } } while(0)
    #define checkf(cond, ...)       do { if(!(cond)) { log_error("Check failed: %s (%s)", #cond, __VA_ARGS__); } } while(0)
    #define fatal_check(cond)       do { if(!(cond)) { log_fatal("Fatal check failed: %s", #cond); } } while(0)
    #define fatal_checkf(cond, ...) do { if(!(cond)) { log_fatal("Fatal check failed: %s (%s)", #cond, __VA_ARGS__); } } while(0)
#else
    #define check(cond)             ((void)0)
    #define checkf(cond, ...)       ((void)0)
    #define fatal_check(cond)       ((void)0)
    #define fatal_checkf(cond, ...) ((void)0)
#endif