#include "basic.h"

// https://prng.di.unimi.it/xoroshiro128plus.c

static inline uint64_t rotl(const uint64_t x, int k) 
{
	return (x << k) | (x >> (64 - k));
}

static uint64_t _seed[2] = { 1, 2 };

static inline uint64_t random_next() 
{
    const uint64_t s0 = _seed[0];
    uint64_t s1 = _seed[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    _seed[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16); // a, b
    _seed[1] = rotl(s1, 37); // c

    return result;
}

static inline bool random_to_bool(uint64_t x) 
{
    return (x & 0x8000000000000000ULL);
}

static inline uint32_t random_to_uint32(uint64_t x) 
{ 
    return x >> 32; 
}

static inline double random_to_double(uint64_t x) 
{ 
    return (x >> 11) * 0x1.0p-53;
}

static inline float random_to_float(uint64_t x) 
{ 
    return (float)random_to_double(x); 
}

static inline uint32_t random_to_uint32_range(uint64_t x, uint32_t min, uint32_t max) 
{ 
    return min + (random_to_uint32(x) % (max + 1 - min)); 
}

static inline uint64_t random_to_uint64_range(uint64_t x, uint64_t min, uint64_t max) 
{ 
    return min + (x % (max + 1 - min));
}

static inline float random_to_float_range(uint64_t x, float min, float max) 
{ 
    return min + (max - min) * random_to_float(x); 
}

static inline double random_to_double_range(uint64_t x, double min, double max) 
{ 
    return min + (max - min) * random_to_double(x); 
}

#define random_bool()            random_to_bool(random_next())
#define random_uint32(min, max)  random_to_uint32_range(random_next(), min, max)
#define random_uint64(min, max)  random_to_uint64_range(random_next(), min, max)
#define random_float(min, max)   random_to_float_range(random_next(), min, max)
#define random_double(min, max)  random_to_double_range(random_next(), min, max)
