/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../vendor/felucca/firmware/hal/smk37_analog_controls.h"
typedef struct {
    smk37_pad_state_t pad[16];
    uint16_t held,pressed;
} gb_smk37_pads_t;
/* Scanner IRQ owns sampling; main consumes under an IRQ lock only when
   an emulated frame can run. Preserve even an attack/release in one sample. */
static inline void gb_smk37_pad_sample(gb_smk37_pads_t *s,unsigned p,unsigned raw){
    if(p>=16)return;
    smk37_pad_change_t c=smk37_pad_sample(&s->pad[p],raw);
    uint16_t bit=(uint16_t)(1u<<p);
    if(c.velocity){s->held|=bit;s->pressed|=bit;}
    if(c.release)s->held&=(uint16_t)~bit;
}
static inline uint16_t gb_smk37_pads_take(gb_smk37_pads_t *s){
    uint16_t value=s->held|s->pressed;s->pressed=0;return value;
}
