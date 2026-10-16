/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "../vendor/felucca/firmware/hal/smk37_analog_controls.h"
/* Absolute Fader 1 volume. Small end deadbands ensure mute/full scale. */
static inline uint32_t smk_volume_sample(uint32_t previous,unsigned input,unsigned raw){
    if(input!=smk37_fader_input[0])return previous;
    if(raw<=8)return 0;
    if(raw>=1015)return 256;
    return (raw-8)*256/1007;
}
/* Apply volume at DMA consumption, so queued audio is muted promptly.
 * One 256-frame ramp (~5.8 ms), with identical gain on both channels.
 * S16 * 256 fits signed 24-bit output, including -32768 at full scale. */
static inline void smk_volume_render(int32_t *out,const int16_t *in,uint32_t start,uint32_t target){
    if(start>256)start=256;
    if(target>256)target=256;
    for(unsigned i=0;i<256;i++){
        int32_t gain=(int32_t)start+((int32_t)target-(int32_t)start)*(int32_t)(i+1)/256;
        out[i*2]=(int32_t)in[i*2]*gain;
        out[i*2+1]=(int32_t)in[i*2+1]*gain;
    }
}
