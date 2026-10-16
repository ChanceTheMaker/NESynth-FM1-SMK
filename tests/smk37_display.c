/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t regs[0x52000/4];
#define SMK37_LCD_REG(a) regs[(a)/4]
#include "../firmware/smk37_display_dma.h"
#include "../firmware/diagnostic_packet.h"
static uint16_t pixels[240*225];
static void display_test(void){
    smk_display_dma_t s={0};smk37_lcd_hw_init(4);
    assert(smk_display_poll(&s,0)==1);
    assert(!smk_display_begin(&s,0,480,0));
    assert(!smk_display_begin(&s,pixels,0,0));
    assert(!smk_display_begin(&s,pixels,108001,0));
    assert(smk_display_begin(&s,pixels,sizeof pixels,100));
    assert(s.busy && !(SMK37_LCD_REG(0x50080)&SMK37_LCD_CS));
    assert(SMK37_LCD_REG(0x50040)&SMK37_LCD_DC);
    assert(SMK37_LCD_REG(0x11d0c)==(uint32_t)(uintptr_t)pixels);
    assert(SMK37_LCD_REG(0x11d10)==sizeof pixels);
    assert(!smk_display_begin(&s,pixels+240,480,200));
    assert(SMK37_LCD_REG(0x11d10)==sizeof pixels && s.started==100);
    assert(smk_display_poll(&s,200)==0 && s.busy);
    /* The scheduler may only submit a new scaler job after ready. */
    unsigned scaler_jobs=0;
    for(unsigned i=201;i<1000;i++)if(smk_display_poll(&s,i)>0)scaler_jobs++;
    assert(!scaler_jobs && s.busy);
    SMK37_LCD_CON|=0x8000;
    assert(smk_display_poll(&s,1000)==1 && !s.busy && s.completed==1 && s.max_ticks==900);
    assert(SMK37_LCD_REG(0x50080)&SMK37_LCD_CS);
    /* Hardware clears the DONE flag on ACK; ordinary mock memory does not. */
    SMK37_LCD_CON&=~0x8000u;
    assert(smk_display_begin(&s,pixels,480,UINT32_MAX-99));
    assert(smk_display_poll(&s,20)==0);
    SMK37_LCD_CON|=0x8000;
    assert(smk_display_poll(&s,50)==1 && s.max_ticks==900 && s.completed==2);
    SMK37_LCD_CON&=~0x8000u;
    assert(smk_display_begin(&s,pixels,480,UINT32_MAX-99));
    assert(smk_display_poll(&s,SMK_LCD_DMA_TIMEOUT-101)==0);
    assert(smk_display_poll(&s,SMK_LCD_DMA_TIMEOUT-100)==-1);
    assert(!s.busy && s.failed && s.timeouts==1 && !(SMK37_LCD_CON&1));
    assert(SMK37_LCD_REG(0x50080)&SMK37_LCD_CS);
    assert(smk_display_poll(&s,0)==-1 && s.timeouts==1);
    smk37_lcd_hw_init(4);assert(!smk_display_begin(&s,pixels,480,0));
    s=(smk_display_dma_t){0};assert(smk_display_begin(&s,pixels,480,0));
    smk_display_stop(&s);assert(!s.busy && !(SMK37_LCD_CON&1));
    assert(!smk_display_begin(&s,pixels,480,0));
}
static void diagnostics_test(void){
    uint32_t values[NP_DIAGNOSTIC_FIELDS];uint8_t out[258];
    for(unsigned i=0;i<NP_DIAGNOSTIC_FIELDS;i++)values[i]=UINT32_MAX-i*0x1234567u;
    const unsigned counts[]={0,16,21,40,44,50};
    for(unsigned v=1;v<=5;v++){
        uint8_t request[]={0x7d,'N','S',(uint8_t)v};
        assert(np_diagnostic_version(request,4)==v);
        assert(!np_diagnostic_version(request,3));
        memset(out,0xcc,sizeof out);unsigned size=6+counts[v]*5;
        assert(!np_diagnostic_packet(out+1,size-1,v,values));
        for(unsigned i=0;i<sizeof out;i++)assert(out[i]==0xcc);
        assert(np_diagnostic_packet(out+1,size,v,values)==size);
        assert(out[0]==0xcc && out[size+1]==0xcc && out[1]==0xf0 && out[size]==0xf7);
        assert(!memcmp(out+2,request,4));
        for(unsigned i=0;i<counts[v];i++){
            uint32_t value=0;
            for(unsigned j=0;j<5;j++){uint8_t b=out[6+i*5+j];assert(b<128);value|=(uint32_t)b<<(7*j);}
            assert(value==values[i]);
        }
    }
    uint8_t invalid[]={0x7d,'N','S',6};
    assert(!np_diagnostic_version(invalid,4) && !np_diagnostic_version(0,4));
    assert(!np_diagnostic_packet(out,sizeof out,0,values));
    assert(!np_diagnostic_packet(out,sizeof out,6,values));
}
int main(void){display_test();diagnostics_test();puts("SMK asynchronous display ownership, timeout, timer wrap and diagnostics passed");return 0;}
