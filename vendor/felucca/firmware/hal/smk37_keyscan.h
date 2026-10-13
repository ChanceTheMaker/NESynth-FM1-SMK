/* SPDX-License-Identifier: GPL-3.0-only */
/* Dual-contact timing recovered from Pro 016 0x0201C188..0x0201C324.
 * Call once per consumer tick, NOT once per SPI slot. The physical tick rate
 * remains to be established; calibration uses counts in that timebase.
 * No hardware access. Initialize state to zero before scanning.
 */
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t elapsed, held_duration;
    uint8_t candidate, accepted, repeats;
} smk37_keyscan_t;

/* raw: bit 0 = first contact, bit 1 = second contact, 1 means closed.
 * Return 1 and set duration on a new press/release; return 0 otherwise.
 * A zero event duration is release. Nonzero is elapsed consumer ticks.
 * Unlike stock's anomalous second-only path, release an existing note when
 * that invalid sequence is accepted, preventing a stuck note.
 */
static inline int smk37_keyscan_tick(smk37_keyscan_t *s, unsigned raw,
                                    uint32_t *duration)
{
    uint32_t before = s->held_duration;
    raw &= 3u;
    if (raw != s->candidate) {
        s->candidate = (uint8_t)raw;
        s->repeats = 0;
    } else if (raw == s->accepted) {
        s->repeats = 0;
    } else if (++s->repeats >= 10) {
        s->repeats = 0;
        if (raw == 2) {
            s->elapsed = 0;
            s->held_duration = 0;
        } else if (s->accepted == 0 || s->accepted == 2) {
            if (raw) {
                s->elapsed = 10;
                if (raw == 3) s->held_duration = 10;
            }
        } else if (s->accepted == 3) {
            s->elapsed = 0;
            if (!raw) s->held_duration = 0;
        } else if (s->accepted == 1) {
            if (!raw) s->held_duration = 0;
            else if (s->elapsed) s->held_duration = s->elapsed;
        }
        s->accepted = (uint8_t)raw;
    }
    /* Zero is deliberately not incremented: during release through the first
     * contact it suppresses a second note if that contact bounces back. */
    if (s->accepted == 1 && s->elapsed && s->elapsed != UINT32_MAX)
        ++s->elapsed;
    if (before == s->held_duration) return 0;
    *duration = s->held_duration;
    return 1;
}
