/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint32_t regs[0x52000/4];
static volatile uint32_t *reg(unsigned address){
    if(address==0x11e10u)assert(regs[0x11e04/4]==95u); /* set before DMA */
    return &regs[address/4];
}
#define SMK37_SCAN_REG(a) (*reg(a))
#include "../firmware/smk37_scan_config.h"
#include "../vendor/felucca/firmware/hal/smk37_scanner.h"
#include "../vendor/felucca/firmware/hal/smk37_analog_scan.h"
static void irq(void){}
static void attach(uint32_t n,void (*fn)(void),uint32_t priority){assert(n==37&&fn==irq&&priority==5);}
int main(void){
    smk37_analog_scan_t analog={0};
    smk37_scan_set_analog(smk37_analog_mask(&analog));
    smk37_scan_indicator_bits=(1u<<16)|(1u<<18); /* PLAY + record */
    smk37_scan_init(attach,irq);
    assert(SMK37_SCAN_CON==0x6021 && SMK37_SCAN_REG(0x11e10)==4);
    assert(smk37_scan_word==(smk37_scan_select[0]&smk37_analog_mask(&analog)));
    assert(!smk37_scan_tick() && smk37_scan_slot==0);
    unsigned frames=0,samples=0;
    for(unsigned slot=0;slot<16*8;slot++){
        /* Every input bit is exercised through the GPIO-to-row mapping. */
        unsigned row=slot&15,pattern=1u<<(row&7);
        SMK37_SCAN_REG(0x50004)=(pattern&1)<<2|(pattern&14)<<4|(pattern&16)<<5;
        SMK37_SCAN_REG(0x501c4)=(pattern&32)<<1|(pattern&192)<<2;
        if(analog.pending){assert(smk37_analog_complete(&analog,analog.index+100));samples++;}
        else smk37_analog_spi_done(&analog);
        smk37_scan_set_analog(smk37_analog_mask(&analog));
        SMK37_SCAN_CON|=0x8000;
        int done=smk37_scan_tick();
        assert(done==(row==15&&slot>=16));frames+=done;
        assert(smk37_scan_frame[row]==pattern);
        assert(smk37_scan_slot==((row+1)&15));
        uint32_t expected=smk37_scan_select[(row+1)&15]&smk37_analog_mask(&analog);
        if(smk37_indicator_pending){
            assert(smk37_indicator_pending==2); /* both transport LEDs share bank 1 */
            expected&=~((1u<<12)|(1u<<11));
        }
        assert(smk37_scan_word==expected);
        assert(!(SMK37_SCAN_REG(0x50000)&2)); /* latch ends low */
        SMK37_SCAN_CON&=~0x8000u; /* mock hardware ACK */
    }
    assert(frames==7&&samples==32&&analog.valid==0xffffff);
    for(unsigned i=0;i<24;i++)assert(analog.raw[i]==i+100);
    smk37_scan_stop();assert(SMK37_SCAN_CON==0x4000);
    puts("PASS: SMK game scan divider before DMA, all rows, latch sequence, warmup and complete analog sweeps.");
}
