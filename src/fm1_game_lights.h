/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "gb_port.h"
/* All piano keys assigned to a held action go dark together. Unassigned
   keys stay dark; releasing the last duplicate restores the whole group. */
static inline uint32_t gp_fm1_key_lights(uint32_t held_actions){
    uint32_t lights=0;
    for(unsigned k=0;k<27;k++){
        uint32_t action=gp_input_keys(1u<<k,0);
        if(action&&!(action&held_actions))lights|=1u<<k;
    }
    return lights;
}
