/* SPDX-License-Identifier: GPL-3.0-only */
/* Link-budget probe ONLY. No real peripheral, flash or radio entry points.
 * core_flash_read fails closed, so even accidental ble_start cannot reach RF.
 * These hooks must never be used for a hardware package. */
#include <stdint.h>
uint32_t core_ms(void){return 0;}
void core_wdt_feed(void){}
void core_reboot(void){for(;;);}
void core_audio_stop(void){}
void core_irq_attach(uint32_t n,void(*h)(void),uint32_t p){(void)n;(void)h;(void)p;}
void core_irq_mask(uint32_t n){(void)n;}
void core_irq_unmask(uint32_t n){(void)n;}
uint32_t core_irq_is_masked(uint32_t n){(void)n;return 1;}
void core_ble_stack_window(void){}
void core_midi_out(uint32_t n){(void)n;}
int core_flash_read(uint32_t off,void *buf,uint32_t n){(void)off;(void)buf;(void)n;return -1;}
uint32_t core_audio_late(void){return 0;}
uint32_t core_ui_frames(void){return 0;}
void core_con_puts(const char *s){(void)s;}
void core_ble_midi_in(uint32_t n){(void)n;}
void core_ble_link(int up){(void)up;}
uint32_t core_ble_in_drops(void){return 0;}
