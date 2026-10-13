/* SPDX-License-Identifier: GPL-3.0-only */
/* SMK-37 Pro 016 digital contact decoder. No register access.
 * Maps recovered from stock tables; see docs/smk37/CONTROL_TRACE.md.
 * Indices are stock firmware order, NOT yet verified physical/MIDI order.
 * Both contact banks must be retained for velocity timing. No debounce or
 * note events are implied by this raw snapshot. The separate application
 * supplies frames from its scanner; physical mapping is still unverified.
 */
#pragma once
#include <stdint.h>

#define SMK37_KEY_COUNT 37u
#define SMK37_ENCODER_COUNT 8u
#define SMK37_BUTTON_COUNT 18u
#define SMK37_SCAN_SLOTS 16u

static const uint8_t smk37_key_contacts[SMK37_KEY_COUNT][2] = {
    {0x10,0x52},{0x20,0x72},{0x30,0x92},{0x40,0xb2},{0x50,0xd2},
    {0x60,0xf2},{0x70,0xe2},{0x80,0xc2},{0x90,0xa2},{0xa0,0x82},
    {0xc0,0x62},{0xd0,0x42},{0xe0,0x22},{0xf0,0x12},{0xb0,0x32},
    {0x11,0x33},{0x21,0x43},{0x31,0x53},{0x41,0x63},{0x51,0x73},
    {0x61,0x83},{0x71,0x93},{0x81,0xa3},{0x91,0xb3},{0xa1,0xc3},
    {0xb1,0xd3},{0xc1,0xe3},{0xd1,0x23},{0xe1,0x13},{0xf1,0x14},
    {0x64,0x24},{0x74,0x34},{0x84,0x44},{0x94,0x54},{0xa4,0xd4},
    {0xb4,0xf4},{0xc4,0xe4}
};
static const uint8_t smk37_encoder_contacts[SMK37_ENCODER_COUNT][2] = {
    {0x97,0xa7},{0x37,0x47},{0x17,0x27},{0xb7,0xc7},
    {0x57,0x67},{0x77,0x87},{0xd7,0xe7},{0xf7,0x07}
};
static const uint8_t smk37_button_contacts[SMK37_BUTTON_COUNT] = {
    0x26,0x76,0x96,0x56,0x66,0x36,0x75,0x65,0x25,
    0x45,0x15,0x86,0x95,0x55,0x85,0x35,0x16,0x46
};

typedef struct {
    uint64_t keys_a, keys_b; /* bits 0..36; no truncation above key 31 */
    uint32_t buttons;       /* bits 0..17 */
    uint16_t encoders;      /* two raw phase bits per encoder */
} smk37_contacts_t;

static inline unsigned smk37_contact_closed(const uint8_t frame[SMK37_SCAN_SLOTS],
                                           uint8_t code)
{
    return !(frame[code >> 4] & (1u << (code & 15u)));
}

/* frame must be a coherent, complete 16-slot sample supplied by the scanner.
 * Return by value; caller controls IRQ synchronization when publishing it. */
static inline smk37_contacts_t smk37_decode_contacts(const uint8_t frame[SMK37_SCAN_SLOTS])
{
    smk37_contacts_t out = {0, 0, 0, 0};
    unsigned i;
    for (i = 0; i < SMK37_KEY_COUNT; ++i) {
        out.keys_a |= (uint64_t)smk37_contact_closed(frame, smk37_key_contacts[i][0]) << i;
        out.keys_b |= (uint64_t)smk37_contact_closed(frame, smk37_key_contacts[i][1]) << i;
    }
    for (i = 0; i < SMK37_BUTTON_COUNT; ++i)
        out.buttons |= (uint32_t)smk37_contact_closed(frame, smk37_button_contacts[i]) << i;
    for (i = 0; i < SMK37_ENCODER_COUNT; ++i) {
        unsigned pair = smk37_contact_closed(frame, smk37_encoder_contacts[i][0]) |
                        (smk37_contact_closed(frame, smk37_encoder_contacts[i][1]) << 1);
        out.encoders |= (uint16_t)(pair << (i * 2));
    }
    return out;
}
