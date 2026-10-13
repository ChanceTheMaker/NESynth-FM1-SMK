/* SPDX-License-Identifier: GPL-3.0-only */
/* Pro 016 panel command sequence; transport calls must be synchronous.
 * Assumes exclusive SPI ownership and an already configured transport.
 * Does not claim a reset/backlight pin or perform any flash access.
 */
#pragma once
#include <stdint.h>

typedef struct {
    int (*command)(uint8_t);
    int (*data)(const void *, uint32_t);
    void (*delay_ms)(uint32_t);
} smk37_panel_io_t;

static inline int smk37_panel_window(const smk37_panel_io_t *io,
                                     unsigned x, unsigned y,
                                     unsigned width, unsigned height)
{
    uint8_t range[4];
    if (!io || !io->command || !io->data || x >= 240 || y >= 240 ||
        !width || !height || width > 240-x || height > 240-y) return 0;
    range[0] = 0; range[1] = (uint8_t)x;
    range[2] = 0; range[3] = (uint8_t)(x + width - 1);
    if (!io->command(0x2a) || !io->data(range, 4)) return 0;
    range[1] = (uint8_t)y;
    range[3] = (uint8_t)(y + height - 1);
    return io->command(0x2b) && io->data(range, 4) && io->command(0x2c);
}

static inline int smk37_panel_init(const smk37_panel_io_t *io)
{
    static const uint8_t settings[][16] = {
        {0x36,1,0x00}, {0x3a,1,0x05}, {0xb2,5,0x0c,0x0c,0x00,0x33,0x33},
        {0xb7,1,0x35}, {0xbb,1,0x32}, {0xc2,1,0x01}, {0xc3,1,0x15},
        {0xc4,1,0x20}, {0xc6,1,0x0f}, {0xd0,2,0xa4,0xa1},
        {0xe0,14,0xd0,0x08,0x0e,0x09,0x09,0x05,0x31,0x33,0x48,0x17,0x14,0x15,0x31,0x34},
        {0xe1,14,0xd0,0x08,0x0e,0x09,0x09,0x05,0x31,0x33,0x48,0x17,0x14,0x15,0x31,0x34}
    };
    uint8_t ram[480]; /* one black RGB565 row, also staging for DMA settings */
    unsigned i, j;
    if (!io || !io->command || !io->data || !io->delay_ms) return 0;
    io->delay_ms(100);
    if (!io->command(0x11)) return 0;
    io->delay_ms(120);
    for (i = 0; i < sizeof(settings)/sizeof(settings[0]); ++i) {
        unsigned n = settings[i][1];
        for (j = 0; j < n; ++j) ram[j] = settings[i][j+2];
        if (!io->command(settings[i][0]) || !io->data(ram, n)) return 0;
    }
    if (!io->command(0x21) || !io->command(0x28)) return 0;
    io->delay_ms(50);
    if (!smk37_panel_window(io, 0, 0, 240, 240)) return 0;
    for (j = 0; j < sizeof(ram); ++j) ram[j] = 0;
    for (i = 0; i < 240; ++i)
        if (!io->data(ram, sizeof(ram))) return 0;
    return io->command(0x29);
}
