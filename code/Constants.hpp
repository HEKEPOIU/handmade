#pragma once
#include <stdint.h>

#define internal static
#define local_persist static
#define global_persist static

// some alignment usecase, and more precised control size.
typedef int32_t bool32_t;
typedef float real32_t;
typedef double real64_t;

#define Pi32 3.14159265359f
