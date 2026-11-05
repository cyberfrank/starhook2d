#include "log.h"
#include "os.h"
#include "sprintf.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

static Critical_Section logging_cs = { 0 };

static const char *log_level_strings[] = {
    "INFO",
    "WARNING",
    "ERROR",
    "TRACE",
    "FATAL",
};

enum {
    MAX_NUM_LOG_CALLBACKS = 4,
    LOG_BUFFER_SIZE = 512,
};

log_func log_callbacks[MAX_NUM_LOG_CALLBACKS];
uint32_t num_log_callbacks = 0;

static void force_crash()
{
    // Trigger access violation
    const uint64_t addr = rand() % 1;
    int *p = 0;
    memcpy(&p, &addr, sizeof(p));
    *p = 10;

    // Exit normally if access violation failed
    exit(-1);
}

void register_log_callback(log_func fn)
{
    check(num_log_callbacks < MAX_NUM_LOG_CALLBACKS);
    log_callbacks[num_log_callbacks++] = fn;
}

void log_stdout(int level, const char *msg, uint32_t len)
{
    fprintf(stdout, msg);
    fflush(stdout);
}

void log_print(int level, const char *file, uint32_t line, const char *msg, ...)
{ 
    os_enter_critical_section(&logging_cs);

    time_t t = time(NULL);
    struct tm *time = localtime(&t);

    char time_str[16];
    time_str[strftime(time_str, sizeof(time_str), "%H:%M:%S", time)] = '\0';

    char buffer[LOG_BUFFER_SIZE];
    uint32_t offset = 0;

    va_list args;
    va_start(args, msg);
    {
        if (level != LOG_TRACE)
        {
            offset += c_print(buffer + offset, LOG_BUFFER_SIZE, "%s %-5s %s:%d: ", time_str, log_level_strings[level], file, line);
        }
        offset += c_vprint(buffer + offset, LOG_BUFFER_SIZE, msg, args);
        offset += c_print(buffer + offset, LOG_BUFFER_SIZE, "\n");
    }
    va_end(args);

    for (uint32_t i = 0; i < num_log_callbacks; ++i)
    {
        log_callbacks[i](level, buffer, offset);
    }

    os_leave_critical_section(&logging_cs);

    if (level == LOG_ERROR || level == LOG_FATAL)
    {
        os_print_stack_trace();
    }

    if (level == LOG_FATAL)
    {
        force_crash();
    }
}
