/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from GameDude: immutable indexed source, CPU1 RGB565 output.
 * CPU0 initializes the palette before launching CPU1. */
#ifndef NES_VIDEO_JOB_H
#define NES_VIDEO_JOB_H
#include <stdint.h>
#include "nes_port.h"
#ifndef GB_VIDEO_RAM
#define GB_VIDEO_RAM
#endif
#ifndef GB_VIDEO_BARRIER
#define GB_VIDEO_BARRIER() __asm__ volatile("" ::: "memory")
#endif
#define GB_VIDEO_PIXELS (256u*240u)
static uint16_t video_palette[64];
static void gb_video_palette_init(void){
    const uint32_t *p=np_palette();
    for(unsigned i=0;i<64;i++){
        uint32_t c=p[i];uint16_t v=(uint16_t)(((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31));
        video_palette[i]=(uint16_t)((v>>8)|(v<<8));
    }
}
typedef struct{volatile uint32_t request,request_inverse,complete,complete_inverse,error,hash,dirty_rows;} gb_video_job_t;
static GB_VIDEO_RAM uint32_t gb_video_hash(const uint8_t *p){
    uint32_t h=2166136261u;for(unsigned i=0;i<GB_VIDEO_PIXELS;i++)h=(h^p[i])*16777619u;return h;
}
static GB_VIDEO_RAM uint32_t gb_video_scale(const uint8_t *src,uint16_t *out){
    unsigned first=225,last=0;
    // Both dimensions scale by 15/16: drop one source pixel per group of 16.
    for(unsigned y=0;y<225;y++){
        const uint8_t *row=src+(y+y/15)*256;
        uint16_t *dst=out+y*240;
        unsigned changed=0;
        for(unsigned group=0;group<16;group++)
            for(unsigned x=0;x<15;x++){
                uint16_t value=video_palette[row[group*16+x]&63];
                changed|=*dst!=value;*dst++=value;
            }
        if(changed){if(first==225)first=y;last=y+1;}
    }
    return (first<<16)|last;
}
static GB_VIDEO_RAM int gb_video_work(gb_video_job_t *job,const uint8_t *src,uint16_t *out){
    uint32_t sequence=job->request;GB_VIDEO_BARRIER();
    if(!sequence||sequence==job->complete)return 0;
    if(job->request_inverse!=~sequence||gb_video_hash(src)!=job->hash){job->error=1;GB_VIDEO_BARRIER();return -1;}
    job->dirty_rows=gb_video_scale(src,out);GB_VIDEO_BARRIER();job->complete_inverse=~sequence;GB_VIDEO_BARRIER();job->complete=sequence;GB_VIDEO_BARRIER();return 1;
}
static int gb_video_poll(const gb_video_job_t *job,uint32_t pending,uint32_t elapsed,uint32_t limit,int fault){
    if(!pending)return 0;
    if(fault||job->error)return -1;
    if(job->complete==pending&&job->complete_inverse==~pending){GB_VIDEO_BARRIER();return 1;}
    return elapsed>limit?-1:0;
}
#endif
