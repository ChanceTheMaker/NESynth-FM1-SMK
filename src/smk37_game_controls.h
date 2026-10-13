/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "gb_port.h"
/* C3..C6 order used by the MusicCoach SMK37 keyboard consumer.
   White/black numbers count each color separately from the left. */
static inline uint32_t gp_smk37_keys(uint32_t lo,uint32_t hi){
    uint32_t p=0;
    if(lo&((1u<<5)|(1u<<7)))p|=GP_LEFT;   /* whites 4/5 */
    if(lo&(1u<<9))p|=GP_DOWN;   /* white 6 */
    if(lo&((1u<<11)|(1u<<12)))p|=GP_RIGHT; /* whites 7/8 */
    if(lo&((1u<<8)|(1u<<10)))p|=GP_UP; /* black 4/5 */
    if(hi&(1u<<1))p|=GP_A;     /* white 20, chromatic key 33 */
    if(hi&(1u<<4))p|=GP_B;     /* white 22, chromatic key 36 */
    return p;
}
/* Physical pads: top row indices 0..7, bottom 8..15, left to right.
   Matches the owner-verified smk37_pad_input permutation. */
static inline uint32_t gp_smk37_pads(uint32_t pads){
    uint32_t p=0;
    if(pads&(1u<<8))p|=GP_LEFT;
    if(pads&(1u<<9))p|=GP_DOWN;
    if(pads&(1u<<10))p|=GP_RIGHT;
    if(pads&(1u<<1))p|=GP_UP;
    if(pads&(1u<<14))p|=GP_B;
    if(pads&(1u<<15))p|=GP_A;
    return p;
}
static inline uint32_t gp_smk37_panel(uint32_t buttons){
    return ((buttons&(1u<<16))?GP_START:0)|((buttons&(1u<<17))?GP_SELECT:0);
}
static inline uint16_t gp_smk37_pad_lights(void){
    uint16_t mask=0;
    for(unsigned p=0;p<16;p++)if(gp_smk37_pads(1u<<p))mask|=(uint16_t)(1u<<p);
    return mask;
}
