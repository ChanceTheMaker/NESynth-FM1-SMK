/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define NP_DIAGNOSTIC_FIELDS 50u
static inline unsigned np_diagnostic_version(const uint8_t *p,unsigned n){
    return p&&n==4&&p[0]==0x7d&&p[1]=='N'&&p[2]=='S'&&p[3]>=1&&p[3]<=5?p[3]:0;
}
static inline unsigned np_diagnostic_packet(uint8_t *out,unsigned capacity,unsigned version,const uint32_t *values){
    unsigned fields=version==1?16:version==2?21:version==3?40:version==4?44:version==5?50:0;
    if(!fields||capacity<6+fields*5)return 0;
    out[0]=0xf0;out[1]=0x7d;out[2]='N';out[3]='S';out[4]=(uint8_t)version;
    unsigned n=5;for(unsigned i=0;i<fields;i++)for(unsigned j=0;j<5;j++)out[n++]=(uint8_t)((values[i]>>(j*7))&127);
    out[n++]=0xf7;return n;
}
