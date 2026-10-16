/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#define NP_BLE_NAME "NESynth SMK Test"
#define NP_BLE_SERVICE_UUID "f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7e0"
#define NP_BLE_VALUE_UUID "f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7e1"
typedef struct { uint16_t handle; uint8_t connected,echo[8]; uint32_t writes; } np_diag_gatt;
void np_diag_connect(np_diag_gatt *,uint16_t);
void np_diag_disconnect(np_diag_gatt *,uint16_t);
int np_diag_event(np_diag_gatt *,uint8_t,const uint8_t *,uint16_t,int);
uint16_t np_diag_read(np_diag_gatt *,uint16_t,uint16_t,uint16_t,uint8_t *,uint16_t);
int np_diag_write(np_diag_gatt *,uint16_t,uint16_t,uint16_t,uint16_t,const uint8_t *,uint16_t);
extern const uint8_t np_diag_db[],np_diag_adv[],np_diag_rsp[];
extern const unsigned np_diag_adv_size,np_diag_rsp_size;
