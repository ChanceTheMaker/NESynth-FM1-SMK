/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Felucca's quadratic 10-bit MASTER curve, with a bottom deadband. */
static inline uint32_t gb_master_gain(uint32_t raw){
    if(raw<=8)return 0;
    if(raw>1023)raw=1023;
    return (raw*raw)>>8;
}
/* S16 -> signed 24-bit DMA, with 6 dB of headroom at full MASTER. */
static inline int32_t gb_master_sample(int16_t sample,uint32_t gain){
    if(gain>4096)gain=4096;
    return (int32_t)sample*(int32_t)gain/32;
}
