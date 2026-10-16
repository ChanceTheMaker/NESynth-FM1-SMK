/* SPDX-License-Identifier: GPL-3.0-only */
/* Format evidence: DEADACTIVE's Hortator ble_vm.c (7e341597), and the
 * pinned JieLi cfg_tool.a vm_init/record traversal. This independent reader
 * deliberately rejects transitional bank states that the SDK would repair. */
#include "rf_calibration.h"
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t crc(uint16_t c,const uint8_t *p,uint32_t n){
    while(n--){c^=(uint16_t)*p++<<8;for(unsigned k=0;k<8;k++)c=(uint16_t)((c&0x8000)?(c<<1)^0x1021:c<<1);}
    return c;
}
static int load_bank(np_rf_read read,void *ctx,np_rf_calibration *out,uint32_t bank,uint32_t size){
    np_rf_calibration next={0};uint8_t head[4],buffer[68];uint32_t seen=0;
    *out=next;
    if(!read)return NP_RF_IO;
    if(read(ctx,bank,head,4))return NP_RF_IO;
    if(word(head)!=0x55aaaa55u)return NP_RF_LAYOUT;
    next.bank=bank;uint32_t pos=bank+4,end=bank+size;
    for(;;){
        if(end-pos<4)return NP_RF_CORRUPT;
        if(read(ctx,pos,head,4))return NP_RF_IO;
        if(word(head)==0xffffffffu)break;
        uint32_t length=((uint32_t)head[2]|(uint32_t)head[3]<<8)>>4;
        uint32_t id=head[1]|(uint32_t)(head[2]&15)<<8;
        if(!id||id>255||!length||length>end-pos-4)return NP_RF_CORRUPT;
        uint16_t check=0;
        for(uint32_t off=0;off<length;){
            uint32_t n=length-off;if(n>sizeof buffer)n=sizeof buffer;
            if(read(ctx,pos+4+off,buffer,n))return NP_RF_IO;
            check=crc(check,buffer,n);off+=n;
        }
        if((check&255)!=head[0])return NP_RF_CORRUPT;
        if(id==187){
            if(length!=68||!word(buffer+64)||word(buffer+64)!=crc(0,buffer,64))return NP_RF_CORRUPT;
            for(unsigned i=0;i<68;i++)next.trim[i]=buffer[i];
            next.record=pos;seen|=1;
        }else if(id==106){
            if(length!=2)return NP_RF_CORRUPT;
            next.xosc[0]=buffer[0];next.xosc[1]=buffer[1];seen|=2;
        }else if(id==107){
            if(length!=7)return NP_RF_CORRUPT;
            for(unsigned i=0;i<7;i++)next.pa[i]=buffer[i];
            seen|=4;
        }
        next.records++;pos+=4+length;
    }
    if(seen!=7)return NP_RF_MISSING;
    *out=next;return NP_RF_OK;
}

int np_rf_load_smk(np_rf_read read,void *ctx,np_rf_calibration *out){
    uint8_t head[4];uint32_t magic[2];*out=(np_rf_calibration){0};
    if(!read)return NP_RF_IO;
    for(unsigned i=0;i<2;i++){if(read(ctx,0x9d000u+i*0x10000u,head,4))return NP_RF_IO;magic[i]=word(head);}
    if(magic[0]==0x55aaaa55u&&(magic[1]==0xffffffffu||magic[1]==0))return load_bank(read,ctx,out,0x9d000u,0x10000u);
    if(magic[1]==0x55aaaa55u&&(magic[0]==0xffffffffu||magic[0]==0))return load_bank(read,ctx,out,0xad000u,0x10000u);
    return NP_RF_LAYOUT;
}
int np_rf_load_fm1(np_rf_read read,void *ctx,np_rf_calibration *out){
    return load_bank(read,ctx,out,0xe8000u,0x1000u);
}
