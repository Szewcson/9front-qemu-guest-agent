/* Linux-only native ABI shim, never installed or used by the 9front build. */
#ifndef LIBJSON_HOST_U_H
#define LIBJSON_HOST_U_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
typedef uint8_t uchar;
typedef uint32_t ulong;
typedef uint32_t Rune;
typedef int64_t vlong;
typedef uint64_t uvlong;
#ifndef nil
#define nil ((void*)0)
#endif
enum { UTFmax=4, Runeerror=0xFFFD, Runemax=0x10FFFF };
#endif
