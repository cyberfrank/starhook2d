#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define _Atomic(x) x

typedef uint64_t AtomicU64;
typedef uint32_t AtomicU32;

static inline uint64_t atomic_fetch_add_64(volatile AtomicU64 *obj, uint64_t value)
{
    return InterlockedExchangeAdd64((volatile LONG64 *)obj, value);
}

static inline uint64_t atomic_fetch_sub_64(volatile AtomicU64 *obj, uint64_t value)
{
    return InterlockedExchangeAdd64((volatile LONG64 *)obj, -(int64_t)value);
}

static inline uint32_t atomic_fetch_add_32(volatile AtomicU32 *obj, uint32_t value)
{
    return InterlockedExchangeAdd((volatile LONG *)obj, value);
}

static inline uint32_t atomic_fetch_sub_32(volatile AtomicU32 *obj, uint32_t value)
{
    return InterlockedExchangeAdd((volatile LONG *)obj, -(int32_t)value);
}