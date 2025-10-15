#ifndef COMMON_USED_DEF_H
#define COMMON_USED_DEF_H

#include <stdint.h>

#define internal static
#define local_variable static
#define global_variable static
#define PI32 3.14159265358979323846f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Kilobytes(Value) ((uint64)((Value) * 1024l))
#define Megabytes(Value) ((uint64)(Kilobytes(Value) * 1024l))
#define Gigabytes(Value) ((uint64)(Megabytes(Value) * 1024l))
#define Terabytes(Value) ((uint64)(Gigabytes(Value) * 1024l))

#if HANDMADE_SLOW
#define Assert(Expression)                                                     \
  if (!(Expression)) {                                                         \
    *(int *)0 = 0;                                                             \
  }
#else
#define Assert(Expression)
#endif

inline uint32 SafeTruncateUInt64(uint64 Value) {
  Assert(Value <= 0xFFFFFFFF);
  uint32 Result = (uint32)Value;

  return Result;
}

#endif
