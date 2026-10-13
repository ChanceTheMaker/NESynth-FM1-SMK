/* SPDX-License-Identifier: GPL-3.0-only */
/* RAM-retained startup policy. No hardware access and no flash persistence.
 * A watchdog reset must retain this RAM for the policy to detect a retry.
 * The requested software-update fallback still needs validation on SMK hardware.
 */
#pragma once
#include <stdint.h>
#define SMK37_BOOT_MAGIC 0x534d4b42u
enum smk37_boot_stage {
    SMK37_BOOT_ENTRY = 1, SMK37_BOOT_MEMORY, SMK37_BOOT_USB,
    SMK37_BOOT_DISPLAY, SMK37_BOOT_SCANNER, SMK37_BOOT_AUDIO,
    SMK37_BOOT_RUNNING, SMK37_BOOT_RECOVERY
};
typedef struct {
    uint32_t magic, inverse, pending, failures, stage, previous_stage;
} smk37_bootguard_t;

/* Called before clearing BSS or touching board peripherals. After two failed
 * attempts, stay in recovery policy until cold power-on or explicit rearm.
 * Corrupt/random retained memory cannot manufacture a valid failure count. */
static inline int smk37_bootguard_begin(volatile smk37_bootguard_t *state, int cold)
{
    if (cold || state->magic != SMK37_BOOT_MAGIC ||
        state->inverse != ~SMK37_BOOT_MAGIC || state->pending > 1 ||
        state->failures > 2 || state->stage > SMK37_BOOT_RECOVERY) {
        state->magic = SMK37_BOOT_MAGIC;
        state->inverse = ~SMK37_BOOT_MAGIC;
        state->pending = state->failures = state->stage = 0;
    }
    state->previous_stage = state->stage;
    if (state->pending && state->failures < 2) ++state->failures;
    if (state->failures >= 2) {
        state->stage = SMK37_BOOT_RECOVERY;
        return 1;
    }
    state->pending = 1;
    state->stage = SMK37_BOOT_ENTRY;
    return 0;
}
static inline void smk37_bootguard_healthy(volatile smk37_bootguard_t *state)
{
    state->failures = state->pending = 0;
    state->stage = SMK37_BOOT_RUNNING;
}
/* An intentional USB update request is not a startup crash. */
static inline void smk37_bootguard_update(volatile smk37_bootguard_t *state)
{
    state->failures = state->pending = 0;
    state->stage = SMK37_BOOT_RECOVERY;
}
