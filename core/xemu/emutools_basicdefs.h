/* K4510 shim standing in for Xemu's emutools_basicdefs.h.
 *
 * cpu65.c is taken from Xemu byte-for-byte; this header is the entire
 * environment it needs from us. Keep it minimal so upstream cpu65.c
 * can be dropped in fresh at any time.
 */
#ifndef K4510_XEMU_SHIM_H
#define K4510_XEMU_SHIM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t   Sint8;
typedef int16_t  Sint16;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;

#define XEMU_LIKELY(x)    __builtin_expect(!!(x), 1)
#define XEMU_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#define XEMU_INLINE       __attribute__((__always_inline__)) inline
#define XEMU_UNREACHABLE() __builtin_unreachable()

#define NL "\n"
#define DEBUGPRINT(...) fprintf(stderr, __VA_ARGS__)
#ifdef DEBUG_CPU
#define DEBUG(...)      fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

/* --- CPU configuration: the MEGA65 build of the core, i.e. a 45GS02 --- */
#define CPU_65CE02
#define MEGA65
#define CPU65_65CE02_6502NMOS_TIMING_EMULATION
#define CPU_STEP_MULTI_OPS
#define CPU65 cpu65
#define M65_CPU_ALWAYS_BUG_JMP_INDIRECT          0
#define M65_CPU_ALWAYS_BUG_NO_RESET_PFD_ON_INT   0
#define M65_CPU_ALWAYS_BUG_BCD                   0
#define M65_CPU_NMOS_ONLY_BUG_JMP_INDIRECT       1
#define M65_CPU_NMOS_ONLY_BUG_NO_RESET_PFD_ON_INT 1
#define M65_CPU_NMOS_ONLY_BUG_BCD                1

/* Declared by Xemu only in the MEGA65 target's custom header; the core
 * uses them for the 32-bit Q-register memory forms, so we declare them. */
extern void   cpu65_write_linear_long_opcode_callback(const Uint8 index, Uint32 data);
extern Uint32 cpu65_read_linear_long_opcode_callback(const Uint8 index);

#endif
