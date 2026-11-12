#ifndef SAVEIRO_TYPES_H
#define SAVEIRO_TYPES_H

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long long  uint64_t;
typedef signed long long    int64_t;

typedef unsigned int        size_t;
typedef int                 ssize_t;
typedef uint32_t            uintptr_t;

#define NULL ((void *)0)

typedef enum { false = 0, true = 1 } bool;

#define UNUSED(x) ((void)(x))

#endif
