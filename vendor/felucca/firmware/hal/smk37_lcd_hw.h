/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro SPI1 display transport from stock Pro 016 register traces.
 * PC8 CS, PB5 D/C, PB6 CLK, PB7 DO (SPI1 route A).
 * Does not touch PA2 (a key input) or assume an FM-1 backlight pin.
 * Caller owns SPI1 exclusively during transfers; stock also uses it for LEDs.
 * No hardware execution has validated these static mappings yet.
 */
#pragma once
#include <stdint.h>

#ifndef SMK37_LCD_REG
#define SMK37_LCD_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#define SMK37_LCD_CON SMK37_LCD_REG(0x11d00u)
#define SMK37_LCD_CS (1u << 8)
#define SMK37_LCD_DC (1u << 5)

static inline void smk37_lcd_deselect(void)
{
    SMK37_LCD_REG(0x50080u) |= SMK37_LCD_CS;
}

/* poll_limit bounds a failed peripheral wait; it is not a millisecond value.
 * On failure disable SPI DMA/interrupts and deselect the panel. */
static inline int smk37_lcd_wait(uint32_t poll_limit)
{
    while (poll_limit--) {
        if (SMK37_LCD_CON & 0x8000u) {
            SMK37_LCD_CON |= 0x4000u;
            smk37_lcd_deselect();
            return 1;
        }
    }
    SMK37_LCD_CON = 0x4000u;
    smk37_lcd_deselect();
    return 0;
}

/* Clock rate is supplied by board startup after clock configuration. */
static inline void smk37_lcd_hw_init(uint32_t baud)
{
    SMK37_LCD_CON = 0x4000u;
    smk37_lcd_deselect();
    SMK37_LCD_REG(0x5008cu) |= SMK37_LCD_CS;
    SMK37_LCD_REG(0x50088u) &= ~SMK37_LCD_CS;
    SMK37_LCD_REG(0x50040u) &= ~(SMK37_LCD_DC | (1u << 6));
    SMK37_LCD_REG(0x5004cu) |= 0xe0u;
    /* Stock configures PB5/6 as outputs and PB7 direction=1 before SPI. */
    SMK37_LCD_REG(0x50048u) = (SMK37_LCD_REG(0x50048u) & ~0x60u) | 0x80u;
    SMK37_LCD_REG(0x51020u) &= ~0x10u;
    SMK37_LCD_REG(0x11d04u) = baud;
    SMK37_LCD_CON = 0x4021u;
}

static inline int smk37_lcd_command(uint8_t command, uint32_t poll_limit)
{
    if (!(SMK37_LCD_CON & 1u) || !poll_limit) return 0;
    SMK37_LCD_REG(0x50040u) &= ~SMK37_LCD_DC;
    SMK37_LCD_REG(0x50080u) &= ~SMK37_LCD_CS;
    SMK37_LCD_CON |= 0x4000u;
    SMK37_LCD_REG(0x11d08u) = command;
    return smk37_lcd_wait(poll_limit);
}

/* data must remain in DMA-accessible RAM until this synchronous call returns. */
static inline int smk37_lcd_data(const void *data, uint32_t bytes,
                                uint32_t poll_limit)
{
    if (!data || !bytes || bytes > 115200u || !poll_limit ||
        !(SMK37_LCD_CON & 1u)) return 0;
    SMK37_LCD_REG(0x50040u) |= SMK37_LCD_DC;
    SMK37_LCD_REG(0x50080u) &= ~SMK37_LCD_CS;
    SMK37_LCD_CON |= 0x4000u;
    SMK37_LCD_REG(0x11d0cu) = (uint32_t)(uintptr_t)data;
    SMK37_LCD_REG(0x11d10u) = bytes;
    return smk37_lcd_wait(poll_limit);
}
