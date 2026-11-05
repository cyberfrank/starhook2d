#pragma once
#include "basic.h"

struct Allocator;

enum {
    FILE_PATH_MAX_LEN = 260,
};

// Types

typedef struct Critical_Section {
    uint64_t opaque;
} Critical_Section;

typedef struct Semaphore_Handle {
    uint64_t opaque;
} Semaphore_Handle;

typedef struct Thread_Handle {
    uint64_t opaque[2];
} Thread_Handle;

typedef struct Fiber_Handle {
    uint64_t opaque;
} Fiber_Handle;

typedef struct File_Handle {
    uint64_t handle;
    bool valid;
} File_Handle;

typedef struct File_Info {
    char path[FILE_PATH_MAX_LEN];
} File_Info;

typedef void thread_entry_func(void *user_data);
typedef void fiber_entry_func(void *user_data);

// Helpers

void *os_read_entire_file(const char *path, uint64_t *size, struct Allocator *a);
void os_write_to_file(const char *path, const uint8_t *data, uint64_t size);

// System info

uint32_t os_num_logical_processors();

// Threads

uint32_t os_thread_id();
uint32_t os_thread_id_from_handle(Thread_Handle handle);

Thread_Handle os_create_thread(thread_entry_func *entry, void *user_data, uint32_t stack_size);
void os_wait_for_thread(Thread_Handle handle);
void os_sleep(double seconds);
void os_yield_processor();

// Fibers

Fiber_Handle os_convert_thread_to_fiber(void *user_data);
void os_convert_fiber_to_thread();
Fiber_Handle os_create_fiber(fiber_entry_func *entry, void *user_data, uint32_t stack_size);
void os_destroy_fiber(Fiber_Handle handle);
void os_switch_to_fiber(Fiber_Handle handle);
void *os_fiber_user_data();

// Critical section

void os_create_critical_section(Critical_Section *cs);
void os_enter_critical_section(Critical_Section *cs);
void os_leave_critical_section(Critical_Section *cs);

// Semaphores

Semaphore_Handle os_create_semaphore(uint32_t initial_count);
void os_semaphore_add(Semaphore_Handle handle, uint32_t count);
void os_semaphore_wait(Semaphore_Handle handle);
bool os_semaphore_poll(Semaphore_Handle handle);
void os_destroy_semaphore(Semaphore_Handle handle);

// Time

Time_Stamp os_time_now();
double os_time_delta(Time_Stamp to, Time_Stamp from);
Time_Stamp os_time_add(Time_Stamp from, double seconds);

// File IO

File_Handle os_open_file_input(const char *path);
File_Handle os_open_file_output(const char *path);
File_Handle os_open_file_append(const char *path);
void os_file_set_position(File_Handle file, uint64_t pos);
uint64_t os_file_size(File_Handle file);
int64_t os_read_file(File_Handle file, void *buffer, uint64_t size);
int64_t os_read_file_at(File_Handle file, uint64_t offset, void *buffer, uint64_t size);
bool os_write_file(File_Handle file, const void *buffer, uint64_t size);
bool os_write_file_at(File_Handle file, uint64_t offset, const void *buffer, uint64_t size);
void os_close_file(File_Handle file);

// Virtual memory

void *os_reserve(uint64_t size);
void os_release(void *mem);
void os_commit(void *mem, uint64_t size);
void os_decommit(void *mem, uint64_t size);

// Debugging

void os_print_stack_trace();

// File management

void os_find_files_recursive(const char *root, File_Info **files, struct Allocator *a);

// Windows / dialogs

String8 os_get_clipboard_text_utf8(struct Allocator *a);
void os_set_clipboard_text_utf8(struct Allocator *a, String8 data);

String8 os_open_file_dialog(struct Allocator *a);
String8 os_save_file_dialog(struct Allocator *a);
