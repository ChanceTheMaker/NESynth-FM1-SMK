/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "smk37_controls.h"
typedef struct {
    uint32_t held, seen, presses, releases;
    uint8_t last_press, last_edge, down;
} smk37_button_watch_t;
static inline void smk37_button_watch_update(volatile smk37_button_watch_t *s,
                                           const smk37_control_changes_t *c)
{
    for (unsigned i=0; i<SMK37_BUTTON_COUNT; ++i) {
        uint32_t bit = 1u<<i;
        if (c->pressed&bit) {
            s->held |= bit; s->seen |= bit; ++s->presses;
            s->last_press = s->last_edge = (uint8_t)(i+1); s->down=1;
        }
        if (c->released&bit) {
            s->held &= ~bit; ++s->releases;
            s->last_edge = (uint8_t)(i+1); s->down=0;
        }
    }
}
