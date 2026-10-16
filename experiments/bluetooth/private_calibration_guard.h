/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define NP_PRIVATE_FACTORY_BYTES 8192u
typedef int (*np_factory_read)(void *,uint32_t,uint8_t *,uint32_t);
static inline int np_private_factory_matches(np_factory_read read,void *ctx,const uint8_t *expected,unsigned n){
    uint8_t buffer[128];
    if(!read||!expected||n!=NP_PRIVATE_FACTORY_BYTES)return 0;
    for(unsigned off=0;off<n;off+=sizeof buffer){
        if(read(ctx,0xc0000u+off,buffer,sizeof buffer))return 0;
        for(unsigned j=0;j<sizeof buffer;j++)if(buffer[j]!=expected[off+j])return 0;
    }
    return 1;
}
