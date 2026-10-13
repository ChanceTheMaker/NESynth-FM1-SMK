/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Linker ranges are word-aligned, disjoint, and exclude retained boot state,
 * stacks and the SPL mailbox. Volatile stores prevent early startup loops
 * from being replaced by a libc call before libc's storage is initialized. */
static inline void smk37_memory_zero(volatile uint32_t *begin,
                                     volatile uint32_t *end)
{
    while (begin != end) *begin++ = 0;
}
static inline void smk37_memory_copy(volatile uint32_t *begin,
                                     volatile uint32_t *end,
                                     const uint32_t *source)
{
    while (begin != end) *begin++ = *source++;
}
