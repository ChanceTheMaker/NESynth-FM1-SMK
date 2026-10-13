/* SPDX-License-Identifier: GPL-3.0-only
 * Derived from fm1_audio.h, Copyright (C) 2026 Leo Kuroshita
 * (@kurogedelic), Hugelton Instruments.
 */
/* SMK-37 Pro ALNK0 channel 3 transport, traced from stock Pro 016.
 * Requires configured audio clocks, zeroed DMA storage, and interrupts masked.
 * Board startup supplies delay/IRQ hooks: this does not assume FM-1 vectors,
 * timer initialization or clock setup. Not yet enabled in an SMK build.
 * Buffer: two halves of half_words int32_t (interleaved L/R).
 */
#pragma once
#include <stdint.h>

#ifndef SMK37_AUDIO_R8
#define SMK37_AUDIO_R8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define SMK37_AUDIO_R16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define SMK37_AUDIO_R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#define SMK37_AUDIO_CON0 SMK37_AUDIO_R16(0x12e00u)
#define SMK37_AUDIO_CON1 SMK37_AUDIO_R16(0x12e04u)
#define SMK37_AUDIO_CON2 SMK37_AUDIO_R8(0x12e08u)
#define SMK37_AUDIO_CON3 SMK37_AUDIO_R8(0x12e0cu)
#define SMK37_AUDIO_HALF 0x80u
#define SMK37_AUDIO_IRQ 11u

typedef struct {
    void (*delay_ms)(uint32_t);
    void (*attach_irq)(uint32_t, void (*)(void), uint32_t);
} smk37_audio_hooks_t;

static inline void smk37_audio_pc_out(unsigned bit, unsigned high)
{
    uint32_t mask = 1u << bit;
    SMK37_AUDIO_R32(0x5008cu) |= mask; /* PC digital input enable */
    if (high) SMK37_AUDIO_R32(0x50080u) |= mask;
    else SMK37_AUDIO_R32(0x50080u) &= ~mask;
    SMK37_AUDIO_R32(0x50088u) &= ~mask;
}

/* Reject invalid arguments before touching hardware. The caller owns DMA
 * lifetime and ensures it lies in processor-accessible RAM. */
static inline int smk37_audio_init(int32_t *buffer, uint32_t half_words,
                                   void (*isr)(void), uint32_t priority,
                                   const smk37_audio_hooks_t *hooks)
{
    if (!buffer || ((uintptr_t)buffer & 3u) || !half_words ||
        half_words > 65535 || (half_words & 1u) || !isr || priority > 7 ||
        !hooks || !hooks->delay_ms || !hooks->attach_irq) return 0;
    smk37_audio_pc_out(6, 0);
    hooks->delay_ms(5);
    SMK37_AUDIO_CON0 = 0;
    SMK37_AUDIO_CON1 = 0;
    SMK37_AUDIO_CON2 = 0;
    SMK37_AUDIO_CON3 = 0;
    SMK37_AUDIO_R32(0x51030u) &= ~0xc0u;
    SMK37_AUDIO_CON3 |= 3u;
    smk37_audio_pc_out(0, 1);
    SMK37_AUDIO_CON0 |= 0x100u;
    smk37_audio_pc_out(2, 1);
    smk37_audio_pc_out(1, 1);
    SMK37_AUDIO_CON0 |= 0x80u;
    SMK37_AUDIO_CON0 &= ~0x40u;
    SMK37_AUDIO_R16(0x12e20u) = (uint16_t)half_words;
    SMK37_AUDIO_CON0 &= ~0x600u;
    SMK37_AUDIO_R32(0x10014u) &= 0xfffff0ffu;
    SMK37_AUDIO_CON3 &= ~0x1cu;
    SMK37_AUDIO_CON3 = (uint8_t)((SMK37_AUDIO_CON3 & 0x1fu) | 0x80u);
    SMK37_AUDIO_CON1 |= (1u << 12) | (1u << 14);
    SMK37_AUDIO_R32(0x12e1cu) = (uint32_t)(uintptr_t)buffer;
    smk37_audio_pc_out(6, 1);
    SMK37_AUDIO_CON1 &= ~(1u << 15);
    SMK37_AUDIO_CON2 = 0x0fu;
    hooks->attach_irq(SMK37_AUDIO_IRQ, isr, priority);
    SMK37_AUDIO_CON0 |= 0x800u;
    hooks->delay_ms(5);
    smk37_audio_pc_out(6, 1);
    return 1;
}

static inline uint8_t smk37_audio_pending(void) { return SMK37_AUDIO_CON2; }
static inline unsigned smk37_audio_free_half(void)
{
    return ((SMK37_AUDIO_CON0 >> 15) & 1u) ^ 1u;
}
static inline void smk37_audio_ack_aux(uint8_t pending)
{
    if (pending & 0x10u) SMK37_AUDIO_CON2 |= 1u;
    if (pending & 0x20u) SMK37_AUDIO_CON2 |= 2u;
    if (pending & 0x40u) SMK37_AUDIO_CON2 |= 4u;
}
static inline void smk37_audio_ack_half(void) { SMK37_AUDIO_CON2 |= 8u; }
static inline void smk37_audio_stop(void) { SMK37_AUDIO_CON0 &= ~0x800u; }
