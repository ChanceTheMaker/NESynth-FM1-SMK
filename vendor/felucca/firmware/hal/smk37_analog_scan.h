/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* External-input bring-up schedule derived from Pro016's channel/mux pairs.
 * Deliberately excludes its unresolved internal-monitor secondary 6/7 path.
 * Does not touch registers. Caller serializes SPI/ADC interrupts and applies
 * the selection mask before feeding SPI completions. Index is electrical
 * scan order, NOT a physical pad/fader/wheel identity. */
#define SMK37_ANALOG_INPUTS 24u
typedef struct {
    uint16_t raw[SMK37_ANALOG_INPUTS];
    uint32_t valid;
    uint8_t index, settling, pending;
} smk37_analog_scan_t;

static inline unsigned smk37_analog_channel(const smk37_analog_scan_t *s)
{
    return s->index < 8 ? 1u : s->index < 16 ? 8u : 2u;
}
static inline uint32_t smk37_analog_mask(const smk37_analog_scan_t *s)
{
    unsigned mux = s->index & 7u;
    if (s->index >= 16) {
        /* Primary 16/17 -> mux 4/5; secondary 0..5 -> mux 0..3/6/7. */
        static const uint8_t order[8] = {4,5,0,1,2,3,6,7};
        mux = order[mux];
    }
    return UINT32_C(0xffffffc7) | (mux << 3);
}
/* Called at the beginning of a completed SPI transfer, matching stock's
 * handshake 1 -> 2 -> 3 -> conversion. Later completions cannot retrigger an
 * outstanding conversion. Zero-initialize before applying the first mask. */
static inline int smk37_analog_spi_done(smk37_analog_scan_t *s)
{
    if (s->pending) return 0;
    if (++s->settling < 3) return 0;
    s->pending = 1;
    return 1;
}
/* Accept one conversion only after it was started. Advance selection after
 * storing the sample under the OLD index, never under the next mux position. */
static inline int smk37_analog_complete(smk37_analog_scan_t *s, uint16_t raw)
{
    if (!s->pending) return 0;
    s->raw[s->index] = raw;
    s->valid |= UINT32_C(1) << s->index;
    s->index = (uint8_t)((s->index + 1u) % SMK37_ANALOG_INPUTS);
    s->settling = s->pending = 0;
    return 1;
}
