#if defined(OS_WINDOWS)

#include "os.h"
#include "array.h"
#include "unicode.h"
#include "allocator.h"
#include "atomics.inl"
#include "log.h"
#include "string_util.h"
#include "sprintf.h"
#include <stdlib.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <commdlg.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dbghelp.lib")

void *os_read_entire_file(const char *path, uint64_t *size, struct Allocator *a)
{
    File_Handle file = os_open_file_input(path);
    if (!file.valid)
    {
        log_error("Unable to find file '%s'", path);
        return 0;
    }

    uint64_t file_size = os_file_size(file);
    void *buffer = c_alloc(a, file_size);

    int64_t result = os_read_file(file, buffer, file_size);
    check(result != -1);
    check((uint64_t)result == file_size);
    *size = file_size;

    os_close_file(file);
    
    return buffer;
}

void os_write_to_file(const char *path, const uint8_t *data, uint64_t size)
{
    File_Handle file = os_open_file_output(path);
    if (!file.valid)
    {
        log_error("Unable to open file '%s'", path);
        return;
    }
    bool success = os_write_file(file, data, size);
    check(success);
    os_close_file(file);
}

uint32_t os_num_logical_processors()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

uint32_t os_thread_id()
{
    return GetCurrentThreadId();
}

uint32_t os_thread_id_from_handle(Thread_Handle handle)
{
    return (uint32_t)handle.opaque[1];
}

struct Thread_Data
{
    thread_entry_func *entry_point;
    void *user_data;
};

static DWORD __stdcall thread_proc(void *data)
{
    struct Thread_Data *thread_data_ptr = (struct Thread_Data *)data;
    struct Thread_Data thread_data = *thread_data_ptr;
    free(thread_data_ptr);

    thread_data.entry_point(thread_data.user_data);

    return 0;
}

Thread_Handle os_create_thread(thread_entry_func *entry_point, void *user_data, uint32_t stack_size)
{
    struct Thread_Data *thread_data = c_alloc(system_allocator, sizeof(struct Thread_Data));
    thread_data->entry_point = entry_point;
    thread_data->user_data = user_data;

    SECURITY_ATTRIBUTES attr = { 0 };
    attr.bInheritHandle = true;
    attr.lpSecurityDescriptor = 0;
    attr.nLength = sizeof(attr);

    DWORD thread_id;
    HANDLE handle = CreateThread(&attr, stack_size, thread_proc, thread_data, CREATE_SUSPENDED, &thread_id);

    ResumeThread(handle);

    Thread_Handle thread;
    memcpy(&thread, &handle, sizeof(handle));
    thread.opaque[1] = thread_id;

    return thread;
}

void os_wait_for_thread(Thread_Handle thread)
{
    HANDLE handle;
    memcpy(&handle, &thread, sizeof(handle));
    WaitForSingleObject(handle, INFINITE);
}

void os_sleep(double seconds)
{
    Sleep((DWORD)(seconds * 1000.0 + 0.5));
}

void os_yield_processor()
{
    YieldProcessor();
}

typedef struct Fiber_Context {
    fiber_entry_func *entry;
    void *user_data;
} Fiber_Context;

enum {
    MAX_FIBERS = 2048
};
static Fiber_Context fiber_registry[MAX_FIBERS] = { 0 };
static AtomicU32 next_fiber_idx = 0;

static void __stdcall fiber_proc(void *params)
{
    Fiber_Context *context = (Fiber_Context *)params;
    context->entry(context->user_data);
}

Fiber_Handle os_convert_thread_to_fiber(void *user_data)
{
    Fiber_Context *context = &fiber_registry[next_fiber_idx];
    atomic_fetch_sub_32(&next_fiber_idx, 1);
    context->entry = 0;
    context->user_data = user_data;
    void *fiber = ConvertThreadToFiberEx(context, FIBER_FLAG_FLOAT_SWITCH);
    Fiber_Handle handle;
    memcpy(&handle, &fiber, sizeof(fiber));
    return handle;
}

void os_convert_fiber_to_thread()
{
    ConvertFiberToThread();
}

Fiber_Handle os_create_fiber(fiber_entry_func *entry, void *user_data, uint32_t stack_size)
{
    Fiber_Context *context = &fiber_registry[next_fiber_idx];
    atomic_fetch_add_32(&next_fiber_idx, 1);
    context->entry = entry;
    context->user_data = user_data;
    void *fiber = CreateFiberEx(stack_size, stack_size, FIBER_FLAG_FLOAT_SWITCH, fiber_proc, context);
    Fiber_Handle handle;
    memcpy(&handle, &fiber, sizeof(fiber));
    return handle;
}

void os_destroy_fiber(Fiber_Handle handle)
{
    DeleteFiber((void *)handle.opaque);
}

void os_switch_to_fiber(Fiber_Handle handle)
{
    SwitchToFiber((void *)handle.opaque);
}

void *os_fiber_user_data()
{
    if (!IsThreadAFiber())
    {
        return 0;
    }
    Fiber_Context *context = (Fiber_Context *)GetFiberData();
    return context->user_data;
}

void os_create_critical_section(Critical_Section *cs)
{
    *(SRWLOCK *)cs = (SRWLOCK)SRWLOCK_INIT;
}

void os_enter_critical_section(Critical_Section *cs)
{
    AcquireSRWLockExclusive((SRWLOCK *)cs);
}

void os_leave_critical_section(Critical_Section *cs)
{
    ReleaseSRWLockExclusive((SRWLOCK *)cs);
}

Semaphore_Handle os_create_semaphore(uint32_t initial_count)
{
    Semaphore_Handle handle;
    handle.opaque = (uint64_t)CreateSemaphoreW(0, initial_count, INT_MAX, 0);
    return handle;
}

void os_semaphore_add(Semaphore_Handle handle, uint32_t count)
{
    ReleaseSemaphore((HANDLE)handle.opaque, count, 0);
}

void os_semaphore_wait(Semaphore_Handle handle)
{
    WaitForSingleObject((HANDLE)handle.opaque, INFINITE);
}

bool os_semaphore_poll(Semaphore_Handle handle)
{
    return (WaitForSingleObject((HANDLE)handle.opaque, 0) == WAIT_OBJECT_0);
}

void os_destroy_semaphore(Semaphore_Handle handle)
{
    CloseHandle((HANDLE)handle.opaque);
}

Time_Stamp os_time_now()
{
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    Time_Stamp result;
    result.opaque = pc.QuadPart;
    return result;
}

double os_time_delta(Time_Stamp to, Time_Stamp from)
{
    int64_t delta = to.opaque - from.opaque;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (double)delta / (double)freq.QuadPart;
}

Time_Stamp os_time_add(Time_Stamp from, double seconds)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    Time_Stamp result;
    result.opaque = from.opaque + (int64_t)(seconds * freq.QuadPart);
    return result;
}

File_Handle os_open_file_input(const char *path)
{
    String16 wpath = utf8_to_utf16(path, system_allocator);
    HANDLE handle = CreateFileW(wpath.str, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    c_free(system_allocator, wpath.str, (wpath.len + 1) * sizeof(uint16_t));
    return (File_Handle) { .handle = (uint64_t)handle, .valid = handle != INVALID_HANDLE_VALUE };
}

File_Handle os_open_file_output(const char *path)
{
    String16 wpath = utf8_to_utf16(path, system_allocator);
    HANDLE handle = CreateFileW(wpath.str, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
     c_free(system_allocator, wpath.str, (wpath.len + 1) * sizeof(uint16_t));
    return (File_Handle) { .handle = (uint64_t)handle, .valid = handle != INVALID_HANDLE_VALUE };
}

File_Handle os_open_file_append(const char *path)
{
    String16 wpath = utf8_to_utf16(path, system_allocator);
    HANDLE handle = CreateFileW(wpath.str, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    c_free(system_allocator, wpath.str, (wpath.len + 1) * sizeof(uint16_t));
    return (File_Handle) { .handle = (uint64_t)handle, .valid = handle != INVALID_HANDLE_VALUE };
}

void os_file_set_position(File_Handle file, uint64_t pos)
{
    if (!file.valid) return;
    LARGE_INTEGER dist_to_move;
    dist_to_move.QuadPart = pos;
    SetFilePointerEx((HANDLE)file.handle, dist_to_move, 0, FILE_BEGIN);
}

uint64_t os_file_size(File_Handle file)
{
    if (!file.valid) return 0;
    LARGE_INTEGER size;
    GetFileSizeEx((HANDLE)file.handle, &size);
    return size.QuadPart;
}

int64_t os_read_file(File_Handle file, void *buffer, uint64_t size)
{
    return os_read_file_at(file, 0, buffer, size);
}

int64_t os_read_file_at(File_Handle file, uint64_t start_offset, void *buffer, uint64_t size)
{
    if (!file.valid) return -1;
    uint64_t offset = 0;
    while (offset < size)
    {
        const uint32_t bytes_to_read = (size - offset) > UINT32_MAX ? UINT32_MAX : (uint32_t)(size - offset);
        DWORD bytes_read = 0;
        const uint64_t pos = start_offset + offset;
        OVERLAPPED Overlapped = {
            .Offset = (uint32_t)pos,
            .OffsetHigh = (uint32_t)(pos >> 32),
        };
        const bool result = ReadFile((HANDLE)file.handle, (uint8_t *)buffer + offset, bytes_to_read, &bytes_read, &Overlapped);
        offset += bytes_read;
        if (!result)
        {
            return -1;
        }
        if (!bytes_read)
        {
            break;
        }
    }
    return offset;
}

bool os_write_file(File_Handle file, const void *buffer, uint64_t size)
{
    return os_write_file_at(file, 0, buffer, size);
}

bool os_write_file_at(File_Handle file, uint64_t start_offset, const void *buffer, uint64_t size)
{
    if (!file.valid) return false;
    uint64_t offset = 0;
    while (offset < size)
    {
        const uint32_t bytes_to_write = (size - offset) > UINT32_MAX ? UINT32_MAX : (uint32_t)(size - offset);
        DWORD bytes_written = 0;
        const uint64_t pos = start_offset + offset;
        OVERLAPPED Overlapped = {
            .Offset = (uint32_t)pos,
            .OffsetHigh = (uint32_t)(pos >> 32),
        };
        const bool result = WriteFile((HANDLE)file.handle, (uint8_t *)buffer + offset, bytes_to_write, &bytes_written, &Overlapped);
        if (!result)
        {
            return false;
        }
        offset += bytes_written;
    }
    return true;
}

void os_close_file(File_Handle file)
{
    if (!file.valid) return;
    CloseHandle((HANDLE)file.handle);
}

void *os_reserve(uint64_t size)
{
    void *mem = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
    return mem;
}

void os_release(void *mem)
{
    VirtualFree(mem, 0, MEM_RELEASE);
}

void os_commit(void *mem, uint64_t size)
{
    VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE);
}

void os_decommit(void *mem, uint64_t size)
{
    VirtualFree(mem, size, MEM_DECOMMIT);
}

typedef struct Stack_Frame {
    DWORD64 address;
    const char *name;
    const char *module;
    const char *file;
    uint32_t line;
} Stack_Frame;

#define MAX_LONG_PATH (32767)

void os_print_stack_trace()
{
    DWORD machine = IMAGE_FILE_MACHINE_AMD64;
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    if (SymInitialize(process, NULL, TRUE) == FALSE)
    {
        return;
    }

    SymSetOptions(SYMOPT_LOAD_LINES);

    CONTEXT context = { 0 };
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context);

    STACKFRAME frame = { 0 };
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    bool first = true;
    int stack_frame_idx = 0;
    log_trace("Stack Trace:");

    while (StackWalk(machine, process, thread, &frame, &context, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL))
    {
        Stack_Frame f = { 0 };
        f.address = frame.AddrPC.Offset;

        DWORD64 module_base = SymGetModuleBase(process, frame.AddrPC.Offset);
        char module_buf[MAX_LONG_PATH];
        if (module_base && GetModuleFileNameA((HINSTANCE)module_base, module_buf, MAX_LONG_PATH))
        {
            f.module = get_file_name(module_buf);
        }
        else
        {
            f.module = "Unknown Module";
        }

        char symbol_buf[sizeof(IMAGEHLP_SYMBOL) + 255];
        PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbol_buf;
        symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL) + 255;
        symbol->MaxNameLength = 254;

        DWORD64 offset = 0;
        if (SymGetSymFromAddr(process, frame.AddrPC.Offset, &offset, symbol))
        {
            f.name = symbol->Name;
        }
        else
        {
            f.name = "unknown";
        }

        IMAGEHLP_LINE line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

        DWORD offset_line = 0;
        if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset_line, &line))
        {
            f.file = line.FileName;
            f.line = line.LineNumber;
        }
        else
        {
            f.file = "???";
            f.line = 0;
        }

        if (!first)
        {
            log_trace("%-3i %-12s %s() %s:%i", stack_frame_idx, f.module, f.name, f.file, f.line);
        }

        first = false;
        ++stack_frame_idx;
    }
    log_trace("---");
    SymCleanup(process);
}

static inline bool file_exclude_filter(const WIN32_FIND_DATA *data)
{
    // Ignore '.' and '..' directories as well as hidden directories
    return (data->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0 && 
        strncmp(".\0", data->cFileName, 2) && strncmp("..\0", data->cFileName, 3);
}

void os_find_files_recursive(const char *root, File_Info **files, struct Allocator *a)
{
    char search_path[FILE_PATH_MAX_LEN];
    c_print(search_path, FILE_PATH_MAX_LEN, "%s/*", root);

    WIN32_FIND_DATA data;
    HANDLE hFind = FindFirstFile(search_path, &data);
    while (true)
    {
        if (file_exclude_filter(&data))
        {
            File_Info info;
            c_print(info.path, FILE_PATH_MAX_LEN, "%s/%s", root, data.cFileName);

            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
            {
                os_find_files_recursive(info.path, files, a);
            } 
            else 
            {
                array_push(*files, info, a);
            }
        }
        BOOL found = FindNextFile(hFind, &data);
        if (!found || hFind == INVALID_HANDLE_VALUE) break;
    }
}

String8 os_get_clipboard_text_utf8(struct Allocator *a)
{
    String8 res = { 0 };
    OpenClipboard(NULL);
    do {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (!h)
            break;
        uint16_t *wstr = GlobalLock(h);
        if (!wstr)
            break;
        res = utf16_to_utf8(wstr, a);
        GlobalUnlock(h);
    } while (0);
    CloseClipboard();
    return res;
}

void os_set_clipboard_text_utf8(struct Allocator *a, String8 data)
{
    OpenClipboard(NULL);
    EmptyClipboard();

    String16 wstr = utf8_to_utf16_n(data.str, data.len, a);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (wstr.len + 1) * 2);
    char *p = GlobalLock(h);
    memcpy(p, wstr.str, 2 * (wstr.len + 1));
    GlobalUnlock(h);
    SetClipboardData(CF_UNICODETEXT, h);
    
    CloseClipboard();
}

String8 os_open_file_dialog(struct Allocator *a)
{
    OPENFILENAME ofn;
    char sz[FILE_PATH_MAX_LEN];
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = 0;
    ofn.lpstrFile = sz;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = FILE_PATH_MAX_LEN;
    ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    
    if (GetOpenFileName(&ofn)) {
        uint64_t len = strlen(ofn.lpstrFile);
        char *str = c_alloc(a, len);
        memcpy(str, ofn.lpstrFile, len);
        str[len] = '\0';
        for (uint32_t i = 0; i < len; ++i) {
            if (str[i] == '\\') 
                str[i] = '/';
        }
        return (String8) { .str = str, .len = (uint32_t)len };
    }

    return (String8) { 0 };
}

String8 os_save_file_dialog(struct Allocator *a)
{
    OPENFILENAME ofn;
    char sz[FILE_PATH_MAX_LEN];
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = 0;
	ofn.lpstrFile = sz;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = FILE_PATH_MAX_LEN;
	ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileName(&ofn)) {
        uint64_t len = strlen(ofn.lpstrFile);
        char *str = c_alloc(a, len);
        memcpy(str, ofn.lpstrFile, len);
        str[len] = '\0';
        for (uint32_t i = 0; i < len; ++i) {
            if (str[i] == '\\')
                str[i] = '/';
        }
        return (String8) { .str = str, .len = (uint32_t)len };
    }

    return (String8) { 0 };
}

#endif