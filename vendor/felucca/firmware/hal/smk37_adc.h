/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "smk37_analog_scan.h"
#ifndef SMK37_ADC_REG
#define SMK37_ADC_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#define SMK37_ADC_CON SMK37_ADC_REG(0x13100u)
/* Scanner-owned polling transport. ADC IRQ stays disabled; completion is
 * checked at the start of SPI IRQs. No competing ADC/SPI producer state. */
typedef struct {
    smk37_analog_scan_t scan;
    uint16_t waits;
    uint8_t enabled;
    uint32_t samples, timeouts;
} smk37_adc_t;
static inline void smk37_adc_stop(smk37_adc_t *s)
{
    SMK37_ADC_CON = 0;
    s->enabled = s->scan.pending = 0;
}
static inline void smk37_adc_init(smk37_adc_t *s)
{
    const uint32_t pa = (1u<<8)|(1u<<10), pc = 1u<<9;
    *s = (smk37_adc_t){ .scan = { .raw = {0} } };
    SMK37_ADC_CON = 0;
    SMK37_ADC_REG(0x50008u) |= pa;
    SMK37_ADC_REG(0x5000cu) &= ~pa;
    SMK37_ADC_REG(0x50010u) &= ~pa;
    SMK37_ADC_REG(0x50014u) &= ~pa;
    SMK37_ADC_REG(0x50088u) |= pc;
    SMK37_ADC_REG(0x5008cu) &= ~pc;
    SMK37_ADC_REG(0x50090u) &= ~pc;
    SMK37_ADC_REG(0x50094u) &= ~pc;
    SMK37_ADC_REG(0x11900u) &= ~(1u<<14); /* external input, not PMU VBG */
    s->enabled = 1;
}
/* Return 1 on a sample, -1 on bounded failure, 0 otherwise. Caller updates
 * the scanner mux mask AFTER this call, before queuing its next SPI word. */
static inline int smk37_adc_spi_done(smk37_adc_t *s)
{
    if (!s->enabled) return 0;
    if (s->scan.pending) {
        if (SMK37_ADC_CON & 0x80u) {
            uint16_t raw = (uint16_t)(SMK37_ADC_REG(0x13104u) & 0x3ffu);
            SMK37_ADC_CON = 0x40u;
            SMK37_ADC_CON = 0;
            (void)smk37_analog_complete(&s->scan, raw);
            ++s->samples;
            s->waits = 0;
            return 1;
        }
        if (++s->waits >= 128u) {
            ++s->timeouts;
            smk37_adc_stop(s);
            return -1;
        }
        return 0;
    }
    if (smk37_analog_spi_done(&s->scan)) {
        /* Stock divisor/start delay, polling instead of ADC IRQ (bit5=0). */
        SMK37_ADC_CON = 0x405du | (smk37_analog_channel(&s->scan) << 8);
        s->waits = 0;
    }
    return 0;
}
