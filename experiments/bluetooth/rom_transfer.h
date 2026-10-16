/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define NP_TRANSFER_CHUNK 512u
enum { NP_RX_IDLE, NP_RX_RECEIVING, NP_RX_VERIFIED, NP_RX_FAILED };
enum { NP_RX_OK=0, NP_RX_STATE=-1, NP_RX_RANGE=-2, NP_RX_CRC=-3, NP_RX_IO=-4, NP_RX_ROM=-5 };
/* Offsets are relative to a cartridge slot, never physical flash addresses.
 * Backend owns staging and persistent validity. begin must invalidate staging
 * before changing bytes. publish must expose only a completely checked image.
 * loader_board.h provides the exclusive-mode BLE/RAM/flash backend. */
typedef struct {
    void *ctx;
    int (*begin)(void *,uint32_t);
    int (*write)(void *,uint32_t,const uint8_t *,uint32_t);
    int (*read)(void *,uint32_t,uint8_t *,uint32_t);
    int (*validate)(void *,uint32_t);
    int (*publish)(void *,uint32_t,uint32_t);
} np_rx_io;
typedef struct {
    np_rx_io io;
    uint32_t capacity,session,length,expected_crc,received,state;
} np_rx;
uint32_t np_rx_crc32(const uint8_t *,uint32_t);
void np_rx_init(np_rx *,const np_rx_io *,uint32_t);
int np_rx_begin(np_rx *,uint32_t,uint32_t,uint32_t);
int np_rx_data(np_rx *,uint32_t,uint32_t,const uint8_t *,uint32_t,uint32_t);
int np_rx_commit(np_rx *,uint32_t);
void np_rx_abort(np_rx *);
