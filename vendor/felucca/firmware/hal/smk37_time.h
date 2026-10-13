/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK application owns TIMER4. Always establish OSC /1 and a full-width period:
 * an inherited timer with the same source but a different prescaler/period
 * must not be mistaken for our 24 MHz timebase. Does not reprogram PLLs. */
#pragma once
#include <stdint.h>
#ifndef SMK37_TIME_REG
#define SMK37_TIME_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
static inline void smk37_time_init(void)
{
    SMK37_TIME_REG(0x10800u) = 0;
    SMK37_TIME_REG(0x10808u) = 0xffffffffu;
    SMK37_TIME_REG(0x10804u) = 0;
    SMK37_TIME_REG(0x10800u) = 0x4009u;
}
