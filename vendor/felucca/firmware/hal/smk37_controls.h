/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "smk37_contacts.h"

/* Bring-up control decoder, not a reproduction of stock hold/combo behavior.
 * Debounce counts complete scanner frames. Steps are electrical quarter
 * transitions with Pro016 signed-event polarity; all-eight physical direction
 * verification remains part of Test08 acceptance. */
typedef struct {
    uint8_t button_candidate[SMK37_BUTTON_COUNT];
    uint8_t button_repeats[SMK37_BUTTON_COUNT];
    uint32_t buttons;
    uint16_t phases;
    uint8_t initialized;
} smk37_controls_t;
typedef struct {
    uint32_t pressed, released;
    int8_t encoder[SMK37_ENCODER_COUNT];
} smk37_control_changes_t;

static inline smk37_control_changes_t smk37_controls_tick(
    smk37_controls_t *s, const uint8_t frame[SMK37_SCAN_SLOTS])
{
    smk37_control_changes_t out = {0, 0, {0}};
    /* Pro016 0x0201c684..688 computes counter6436 - counter6440.
     * 00 -> 01 -> 11 -> 10 -> 00 increments counter6440, so is NEGATIVE.
     * Test07 used the opposite sign; owner observed clockwise decrementing.
     * Both bits changing at once means a missed/invalid sample: resynchronize. */
    static const int8_t direction[16] = {
        0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0
    };
    uint16_t phases = 0;
    for (unsigned i = 0; i < SMK37_BUTTON_COUNT; ++i) {
        unsigned raw = smk37_contact_closed(frame, smk37_button_contacts[i]);
        uint32_t mask = 1u << i;
        if (raw != s->button_candidate[i]) {
            s->button_candidate[i] = (uint8_t)raw;
            s->button_repeats[i] = 1;
        } else if (s->button_repeats[i] < 3) ++s->button_repeats[i];
        if (s->button_repeats[i] == 3 && raw != !!(s->buttons & mask)) {
            if (raw) { s->buttons |= mask; out.pressed |= mask; }
            else { s->buttons &= ~mask; out.released |= mask; }
        }
    }
    for (unsigned i = 0; i < SMK37_ENCODER_COUNT; ++i) {
        unsigned raw = smk37_contact_closed(frame, smk37_encoder_contacts[i][0]) |
            smk37_contact_closed(frame, smk37_encoder_contacts[i][1]) << 1;
        unsigned previous = (s->phases >> (2*i)) & 3u;
        phases |= (uint16_t)(raw << (2*i));
        if (s->initialized) out.encoder[i] = direction[previous*4+raw];
    }
    s->phases = phases;
    s->initialized = 1;
    return out;
}
