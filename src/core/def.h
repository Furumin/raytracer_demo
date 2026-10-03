#pragma once
// Types
typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

// Casting
#define V_(a) ( (void*) (a) )

#define KILO(a) ((a) * 1024)
#define MEGA(a) ((a) * 1024 * 1024)
#define GIGA(a) ((a) * 1024 * 1024 * 1024)

// Helpers
#if FLAG_GCC_DEBUG
#include <cassert>
#define ASSERT(exp, msg) do {if (!(exp)) { _assert((msg), __FILE__, __LINE__); }} while (0)
#define DEBUGLOG(...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#else
#include <stdlib.h>
#define ASSERT(...)
#define DEBUGLOG(...) 
#endif

// arr must be a static array!
#define ARRAYCOUNT(arr) ( sizeof (arr) / sizeof(arr[0]) )
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(x, a, b) (MAX(a, MIN(x, b)))

// n must be divisible by 2!
#define ALIGN(a, n) (((a) + n - 1) & ~(n - 1))

#define COMPILER_ERROR() static_assert(0, "Unsupported compiler! Only GCC and MSVC are supported!")

#if !(_MSC_VER || __GNUC__)
COMPILER_ERROR();
#endif
