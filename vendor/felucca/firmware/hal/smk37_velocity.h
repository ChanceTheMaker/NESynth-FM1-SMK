/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Measured scan-frame duration, not factory calibration tick assumptions.
 * Generic response endpoints: 2ms fast, 60ms slow. Hardware feel needs testing.
 * Modes: linear, soft, hard, fixed100. A release always stays zero. */
static inline uint8_t smk37_key_velocity(uint32_t frames, uint32_t frame_ticks,
                                        unsigned mode)
{
    if (!frames) return 0;
    if (mode==3 || !frame_ticks) return 100;
    uint32_t us=frames>1440000u/frame_ticks ? 60000u : frames*frame_ticks/24u;
    unsigned v=us<=2000u ? 127u : 1u+(60000u-us)*126u/58000u;
    if(mode==1) v=32u+v*95u/127u;
    else if(mode==2) v=1u+v*v*126u/(127u*127u);
    return (uint8_t)v;
}
