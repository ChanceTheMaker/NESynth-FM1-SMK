/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "../vendor/felucca/firmware/hal/smk37_analog_controls.h"
typedef struct {uint16_t initial;uint8_t ready,active;} smk_brightness_t;
/* Keep the startup level until Fader 2 moves. Deadbands reject ADC chatter. */
static inline uint32_t smk_brightness_sample(smk_brightness_t *s,uint32_t previous,unsigned input,unsigned raw){
    if(input!=smk37_fader_input[1])return previous;
    if(!s->ready){s->initial=(uint16_t)raw;s->ready=1;return previous;}
    if(!s->active){
        unsigned delta=raw>s->initial?raw-s->initial:s->initial-raw;
        if(delta<8)return previous;
        s->active=1;
    }
    uint32_t level=raw<=8?0:raw>=1015?255:(raw-8)*255/1007;
    unsigned delta=level>previous?level-previous:previous-level;
    return !level||level==255||delta>=2?level:previous;
}
