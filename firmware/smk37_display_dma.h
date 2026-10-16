/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../vendor/felucca/firmware/hal/smk37_lcd_hw.h"
#ifndef SMK_LCD_DMA_BARRIER
#define SMK_LCD_DMA_BARRIER() ((void)0)
#endif
/* CPU0 owns SPI1. A pending transfer owns its source buffer until poll returns
 * ready; neither CPU may scale into that buffer in the meantime. */
typedef struct {uint32_t busy,failed,started,max_ticks,completed,timeouts;} smk_display_dma_t;
#define SMK_LCD_DMA_TIMEOUT 6000000u /* 250 ms at the 24 MHz timebase */
static inline void smk_display_stop(smk_display_dma_t *s){
    SMK37_LCD_CON=0x4000u;smk37_lcd_deselect();s->busy=0;
}
static inline int smk_display_poll(smk_display_dma_t *s,uint32_t now){
    if(s->failed)return -1;
    if(!s->busy)return 1;
    uint32_t elapsed=now-s->started;
    if(SMK37_LCD_CON&0x8000u){
        SMK37_LCD_CON|=0x4000u;smk37_lcd_deselect();s->busy=0;s->completed++;
        if(elapsed>s->max_ticks)s->max_ticks=elapsed;
        return 1;
    }
    if(elapsed>=SMK_LCD_DMA_TIMEOUT){s->timeouts++;s->failed=1;smk_display_stop(s);return -1;}
    return 0; /* Never spin on the peripheral. */
}
static inline int smk_display_begin(smk_display_dma_t *s,const void *pixels,uint32_t bytes,uint32_t now){
    if(s->busy||s->failed||!pixels||!bytes||bytes>108000u||!(SMK37_LCD_CON&1u))return 0;
    SMK37_LCD_REG(0x50040u)|=SMK37_LCD_DC;
    SMK37_LCD_REG(0x50080u)&=~SMK37_LCD_CS;
    SMK37_LCD_CON|=0x4000u;
    SMK37_LCD_REG(0x11d0cu)=(uint32_t)(uintptr_t)pixels;
    s->started=now;s->busy=1;SMK_LCD_DMA_BARRIER();
    SMK37_LCD_REG(0x11d10u)=bytes;
    return 1;
}
