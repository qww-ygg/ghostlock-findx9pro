/*
 * Target configuration dispatch.
 *
 * TARGET_CONFIG_H selects the per-device hardware profile header (address space
 * constants, struct field offsets, symbol offsets). It defaults to target.h so
 * a plain `make` / direct clang invocation works; pass
 *   -DTARGET_CONFIG_H=target.h
 * to override.
 *
 * NOTE: on Windows the NDK ships clang as a .cmd wrapper which mangles quoted
 * -D values, so the default below is required there. Do not remove it.
 */
#ifndef TARGET_CONFIG_H
#define TARGET_CONFIG_H "target.h"
#endif

#include TARGET_CONFIG_H
