/* SPDX-License-Identifier: GPL-3.0-only */
/* Preserve the Pro016 SPL handoff before replacing the vector table.
 * Layout follows stock 0x020001e6, not a raw structure memcpy: the input and
 * retained boot_info have different field offsets. No flash writes. */
#pragma once
#include <stdint.h>
static inline void smk37_boot_copy(volatile uint8_t *info,
                                   const uint8_t *parameters,
                                   const uint8_t *flash_header)
{
    unsigned i;
    for (i = 0; i < 8; ++i) info[16+i] = parameters[4+i];
    for (i = 0; i < 4; ++i) info[24+i] = flash_header[8+i];
    info[0] = flash_header[13];
    for (i = 0; i < 12; ++i) info[28+i] = parameters[12+i];
}
#ifndef SMK37_BOOT_HOST_TEST
static inline void smk37_boot_capture(const uint32_t *parameters)
{
    smk37_boot_copy((volatile uint8_t *)(uintptr_t)0x01c7fd50u,
                    (const uint8_t *)parameters,
                    (const uint8_t *)(uintptr_t)parameters[0]);
}
#endif
