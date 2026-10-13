/* SPDX-License-Identifier: GPL-3.0-only */
/* Pro016 SPI2 digital scan, from 0x2004de6 / 0x201be6a.
 * The optional analog selection mask owns only bits 3..5. Its default is
 * 0xffffffff, as in the stock initial RAM image.
 * The stock pipeline samples rows before raising PA1, launches the next
 * four-byte selection, then lowers PA1. Contact tables use that same order.
 * Board-level behavior remains unverified; this is only an integration target.
 */
#pragma once
#include <stdint.h>
#include "smk37_lights.h"
#ifndef SMK37_SCAN_REG
#define SMK37_SCAN_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#define SMK37_SCAN_CON SMK37_SCAN_REG(0x11e00u)
static uint32_t smk37_scan_word __attribute__((aligned(4)));
static uint32_t smk37_scan_analog_mask = UINT32_MAX;
/* Main loop publishes one indicator (1..19); zero is off. ISR owns the
 * pipeline and uses 1/4 duty. Stock common GPIOs 150/151 use 0x16a0c,
 * distinct from the USB0 device controller. See 0x020042de/0x0201c3d2. */
static volatile uint32_t smk37_scan_indicator;
static volatile uint32_t smk37_scan_indicator_bits;
static uint8_t smk37_indicator_phase, smk37_indicator_pending;
static inline void smk37_indicators_blank(void)
{
    SMK37_SCAN_REG(0x16a0cu) |= 0x7000fu;
}
static inline void smk37_scan_set_analog(uint32_t mask)
{
    smk37_scan_analog_mask = mask | UINT32_C(0xffffffc7);
}
static uint8_t smk37_scan_frame[16], smk37_scan_slot, smk37_scan_warm;
static const uint32_t smk37_scan_select[16] = {
    0xfeffffff,0xfdffffff,0xfbffffff,0xf7ffffff,
    0xefffffff,0xdfffffff,0xbfffffff,0x7fffffff,
    0xfffeffff,0xfffdffff,0xfffbffff,0xfff7ffff,
    0xffefffff,0xffdfffff,0xffbfffff,0xff7fffff
};
static inline void smk37_scan_next(void)
{
    smk37_scan_word = smk37_scan_select[smk37_scan_slot] & smk37_scan_analog_mask;
    unsigned indicator = smk37_scan_indicator;
    smk37_indicator_pending = 0;
    if (indicator && indicator <= 19 && !(smk37_indicator_phase++ & 3u)) {
        smk37_scan_word &= smk37_indicator_mask(indicator-1);
        smk37_indicator_pending = 1u + (smk37_indicator_order[indicator-1]>>4);
    } else if (!indicator && smk37_scan_indicator_bits) {
        unsigned phase=smk37_indicator_phase++ & 3u;
        if (!(phase&1u)) {
            unsigned bank=phase>>1;
            uint32_t selected=smk37_scan_indicator_bits, mask=UINT32_MAX;
            for(unsigned i=0;i<19;++i)
                if ((selected&(1u<<i)) && (smk37_indicator_order[i]>>4)==bank)
                    mask &= smk37_indicator_mask(i);
            smk37_scan_word &= mask;
            if (mask!=UINT32_MAX) smk37_indicator_pending=(uint8_t)(bank+1);
        }
    }
    SMK37_SCAN_REG(0x11e0cu) = (uint32_t)(uintptr_t)&smk37_scan_word;
    SMK37_SCAN_REG(0x11e10u) = 4;
}
static inline void smk37_scan_init(void (*attach)(uint32_t, void (*)(void), uint32_t),
                                   void (*isr)(void))
{
    const uint32_t pa_rows = (1u<<2)|(1u<<5)|(1u<<6)|(1u<<7)|(1u<<9);
    const uint32_t ph_rows = (1u<<6)|(1u<<8)|(1u<<9);
    unsigned i;
    SMK37_SCAN_CON = 0x4000;
    smk37_indicators_blank();
    smk37_scan_slot = smk37_scan_warm = 0;
    for (i = 0; i < 16; ++i) smk37_scan_frame[i] = 0xff;
    SMK37_SCAN_REG(0x50008u) |= pa_rows;
    SMK37_SCAN_REG(0x5000cu) |= pa_rows;
    SMK37_SCAN_REG(0x50010u) |= pa_rows;
    SMK37_SCAN_REG(0x50014u) &= ~pa_rows;
    SMK37_SCAN_REG(0x501c8u) |= ph_rows;
    SMK37_SCAN_REG(0x501ccu) |= ph_rows;
    SMK37_SCAN_REG(0x501d0u) |= ph_rows;
    SMK37_SCAN_REG(0x501d4u) &= ~ph_rows;
    SMK37_SCAN_REG(0x50000u) &= ~((1u<<1)|(1u<<3)|(1u<<4));
    SMK37_SCAN_REG(0x50008u) &= ~((1u<<1)|(1u<<3)|(1u<<4));
    SMK37_SCAN_REG(0x5000cu) |= (1u<<1)|(1u<<3)|(1u<<4);
    SMK37_SCAN_REG(0x50010u) &= ~((1u<<1)|(1u<<3)|(1u<<4));
    SMK37_SCAN_REG(0x50014u) &= ~((1u<<1)|(1u<<3)|(1u<<4));
    SMK37_SCAN_REG(0x51020u) = (SMK37_SCAN_REG(0x51020u) & ~(1u<<16)) | (1u<<17);
    SMK37_SCAN_CON = 0x6020;
    SMK37_SCAN_REG(0x11e04u) = 5;
    attach(37, isr, 5);
    smk37_scan_next();
    SMK37_SCAN_CON |= 1;
    /* Pro016 0x02004EC8..0x02004ECA calls gpio_output(0, 0).
     * That helper clears PA0 OUT then DIR (0x020043FA..0x020043FE).
     * Preserve its ordering after SPI starts; external PA0 role is unverified. */
    SMK37_SCAN_REG(0x50000u) &= ~1u;
    SMK37_SCAN_REG(0x50008u) &= ~1u;
}
/* IRQ-only; caller consumes a completed frame before the next scan IRQ. */
static inline int smk37_scan_tick(void)
{
    uint32_t pa, ph;
    if (!(SMK37_SCAN_CON & 0x8000u)) return 0;
    SMK37_SCAN_CON |= 0x4000u;
    pa = SMK37_SCAN_REG(0x50004u);
    ph = SMK37_SCAN_REG(0x501c4u);
    smk37_scan_frame[smk37_scan_slot] = (uint8_t)(
        ((pa>>2)&1) | ((pa>>4)&14) | ((pa>>5)&16) |
        ((ph>>1)&32) | ((ph>>2)&192));
    /* The word queued on the previous IRQ is now latched. Blank before the
     * latch edge, then enable only its matching common bank, avoiding ghosts. */
    smk37_indicators_blank();
    SMK37_SCAN_REG(0x50000u) |= 2;
    if (smk37_indicator_pending && (smk37_scan_indicator || smk37_scan_indicator_bits))
        SMK37_SCAN_REG(0x16a0cu) &= ~(1u << (16u+smk37_indicator_pending));
    smk37_scan_slot = (smk37_scan_slot + 1u) & 15u;
    smk37_scan_next();
    SMK37_SCAN_REG(0x50000u) &= ~2u;
    if (smk37_scan_slot) return 0;
    if (!smk37_scan_warm) { smk37_scan_warm = 1; return 0; }
    return 1;
}
static inline void smk37_scan_stop(void)
{
    smk37_indicators_blank();
    SMK37_SCAN_CON = 0x4000u;
}
