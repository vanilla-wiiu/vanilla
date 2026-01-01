#pragma once

#include <stdint.h>

/*
 * Minimal Linux-compatible type definitions
 * for non-Linux platforms (iOS, macOS).
 *
 * These are only used to satisfy builds that
 * include Linux headers but never execute
 * Linux kernel code on this platform.
 */

typedef uint8_t   __u8;
typedef uint16_t  __u16;
typedef uint32_t  __u32;
typedef uint64_t  __u64;

typedef int8_t    __s8;
typedef int16_t   __s16;
typedef int32_t   __s32;
typedef int64_t   __s64;

/* Common aliases sometimes expected */
typedef __u8  u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

typedef __s8  s8;
typedef __s16 s16;
typedef __s32 s32;
typedef __s64 s64;
