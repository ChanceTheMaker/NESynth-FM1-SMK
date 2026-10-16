/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
enum { NP_RF_OK=0, NP_RF_LAYOUT=-1, NP_RF_IO=-2, NP_RF_CORRUPT=-3, NP_RF_MISSING=-4 };
typedef int (*np_rf_read)(void *,uint32_t,uint8_t *,uint32_t);
typedef struct {
    uint8_t trim[68],xosc[2],pa[7];
    uint32_t bank,record,records;
} np_rf_calibration;
/* Read-only subset of the pinned SDK VM format. Two 64 KiB banks at
 * 0x9d000 and 0xad000, within the SDK's 128 KiB maximum. No VM recovery,
 * defrag, erase, fallback to stale records, or arbitrary byte scanning.
 * Only one committed bank plus an erased/invalidated peer is accepted.
 * Output is cleared on failure. Call before starting the radio. */
/* FM-1: preserved stock VM snapshot in sector 0xe8000, validated end-to-end. */
int np_rf_load_fm1(np_rf_read read,void *ctx,np_rf_calibration *out);
int np_rf_load_smk(np_rf_read read,void *ctx,np_rf_calibration *out);
