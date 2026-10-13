/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* This unit: ADC23 ~871 released, 0 pressed. Hysteresis plus three fresh
 * conversions rejects contact bounce. Arm only after observing release, so
 * a pedal held at startup or an uninitialized input cannot latch sustain. */
typedef struct { uint8_t armed,down,candidate,count; } smk37_pedal_t;
static inline void smk37_pedal_reset(smk37_pedal_t *s)
{
    s->armed=s->down=s->candidate=s->count=0;
}
/* -1: no transition, 0: released, 1: pressed. Scanner owns this state. */
static inline int smk37_pedal_sample(smk37_pedal_t *s,unsigned raw)
{
    if(raw>1023 || (raw>200 && raw<650)){s->count=0;return -1;}
    unsigned down=raw<=200;
    if(down!=s->candidate){s->candidate=(uint8_t)down;s->count=0;}
    if(s->count<3)++s->count;
    if(s->count<3)return -1;
    if(!s->armed){if(!down)s->armed=1;return -1;}
    if(down==s->down)return -1;
    s->down=(uint8_t)down;return (int)down;
}
