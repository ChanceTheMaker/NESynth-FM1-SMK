/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Pro016: RGB setter 0x02004fb0, permutation 0x0205e7a0,
 * serializer 0x02029454. 36 zero bytes, then 16 GRB words, MSB first;
 * each logical bit becomes 0xf0 (one) or 0xc0 (zero). */
#define SMK37_LIGHT_BYTES 420u
static const uint8_t smk37_pad_light_order[16] = {
    0,15,14,13,12,11,10,9,1,2,3,4,5,6,7,8
};
/* Pro016 0x020586bc: 19 indicator entries, high nibble common bank,
 * low nibble active-low shift-register bit. Physical names were mapped by
 * the owner in Test07; see docs/smk37/CONTROL_TRACE.md. */
static const uint8_t smk37_indicator_order[19] = {
    12,18,0,16,13,29,2,1,9,8,11,10,17,25,24,26,28,30,27
};
static inline void smk37_light_encode_mask(uint8_t out[SMK37_LIGHT_BYTES],
                                          uint16_t pads, unsigned color, unsigned level)
{
    if (level > 8) level = 8; /* bring-up brightness ceiling */
    for (unsigned i=0; i<36; ++i) out[i] = 0;
    for (unsigned i=36; i<SMK37_LIGHT_BYTES; ++i) out[i] = 0xc0;
    if (color >= 3) return;
    static const uint8_t shifts[3] = {8,16,0}; /* requested R, G, B */
    uint32_t word = (uint32_t)level << shifts[color];
    for (unsigned pad=0; pad<16; ++pad) if (pads&(1u<<pad)) {
        unsigned start = 36u + 24u*smk37_pad_light_order[pad];
        for (unsigned bit=0; bit<24; ++bit)
            out[start+bit] = word & (1u << (23-bit)) ? 0xf0 : 0xc0;
    }
}
static inline void smk37_light_encode(uint8_t out[SMK37_LIGHT_BYTES],
                                     unsigned pad, unsigned color, unsigned level)
{
    smk37_light_encode_mask(out, pad<16 ? (uint16_t)(1u<<pad) : 0, color, level);
}
/* Individual RGB colors, retaining the existing per-channel brightness cap. */
static inline void smk37_light_encode_rgb(uint8_t out[SMK37_LIGHT_BYTES],const uint32_t rgb[16])
{
    for(unsigned i=0;i<36;i++)out[i]=0;
    for(unsigned pad=0;pad<16;pad++){
        unsigned r=(rgb[pad]>>16)&255,g=(rgb[pad]>>8)&255,b=rgb[pad]&255;
        if(r>8)r=8;if(g>8)g=8;if(b>8)b=8;
        uint32_t word=(g<<16)|(r<<8)|b;unsigned start=36+24*smk37_pad_light_order[pad];
        for(unsigned bit=0;bit<24;bit++)out[start+bit]=word&(1u<<(23-bit))?0xf0:0xc0;
    }
}
static inline uint32_t smk37_indicator_mask(unsigned indicator)
{
    return indicator < 19 ? ~(1u << (smk37_indicator_order[indicator]&15u)) : UINT32_MAX;
}
