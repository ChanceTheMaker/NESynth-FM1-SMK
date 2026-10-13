/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "smk37_analog_scan.h"

/* Diagnostic only. Capture short pad excursions between display refreshes.
 * Sixteen ADC counts is a display activity threshold, NOT pad calibration.
 * Electrical indices must not be presented as verified physical controls. */
#define SMK37_ANALOG_ACTIVITY 16u
typedef struct {
    uint16_t current[SMK37_ANALOG_INPUTS], low[SMK37_ANALOG_INPUTS];
    uint16_t high[SMK37_ANALOG_INPUTS], anchor[SMK37_ANALOG_INPUTS];
    uint32_t valid, changed;
    uint8_t last; /* 1-based; zero means no threshold crossing yet */
} smk37_analog_watch_t;

/* ISR owns writes; main masks interrupts only while copying the snapshot. */
static inline void smk37_analog_watch_update(volatile smk37_analog_watch_t *s,
                                            unsigned index, uint16_t raw)
{
    if (index >= SMK37_ANALOG_INPUTS || raw > 1023u) return;
    uint32_t bit = UINT32_C(1) << index;
    s->current[index] = raw;
    if (!(s->valid & bit)) {
        s->low[index] = s->high[index] = s->anchor[index] = raw;
        s->valid |= bit;
        return;
    }
    if (raw < s->low[index]) s->low[index] = raw;
    if (raw > s->high[index]) s->high[index] = raw;
    if (s->high[index] - s->low[index] >= (int)SMK37_ANALOG_ACTIVITY)
        s->changed |= bit;
    unsigned delta = raw > s->anchor[index] ? raw - s->anchor[index] : s->anchor[index] - raw;
    if (delta >= SMK37_ANALOG_ACTIVITY) {
        s->anchor[index] = raw;
        s->last = (uint8_t)(index + 1u);
    }
}
