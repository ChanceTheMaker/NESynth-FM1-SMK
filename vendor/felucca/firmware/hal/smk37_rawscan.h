/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
typedef struct {
    uint8_t ready, current[16], changed_bits[16];
    uint32_t changed_frames;
} smk37_rawscan_t;
/* ISR owns updates. Main loop must mask interrupts for a coherent snapshot.
 * First complete frame establishes baseline; sticky XOR captures even a press
 * and release between display updates. Counts changed frames, not key events. */
static inline void smk37_rawscan_update(volatile smk37_rawscan_t *s, const uint8_t *frame) {
    unsigned changed = 0;
    for (unsigned i=0; i<16; ++i) {
        uint8_t delta = s->ready ? s->current[i] ^ frame[i] : 0;
        s->changed_bits[i] |= delta;
        changed |= delta;
        s->current[i] = frame[i];
    }
    if (changed && s->changed_frames != UINT32_MAX) ++s->changed_frames;
    s->ready = 1;
}
