/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GB_PORT_RATE 44100u
#define GB_PORT_WIDTH 256u
#define GB_PORT_HEIGHT 240u
enum { GP_A=1, GP_B=2, GP_SELECT=4, GP_START=8, GP_RIGHT=16, GP_LEFT=32, GP_UP=64, GP_DOWN=128 };
enum { GP_OK=0, GP_SHORT=1, GP_FORMAT=2, GP_ROM_SIZE=3, GP_RAM_SIZE=4,
       GP_REGION=5, GP_MAPPER=6, GP_CPU=7, GP_TIMEOUT=8 };
int gp_load(const uint8_t*,uint32_t);
int gp_frame(void);
void gp_buttons(uint32_t);
void gp_pause(uint32_t);
void gp_reset(void);
void gp_volume(uint32_t);
uint8_t *gp_pixels(void);
uint8_t *gp_save_data(void);
uint32_t gp_save_size(void);
uint32_t gp_save_dirty(void);
void gp_save_clean(void);
void gp_audio_read(int16_t*,uint32_t);
uint32_t gp_audio_queued(void);
uint32_t gp_error(void);
uint32_t gp_frame_count(void);
const char *gp_title(void);
uint32_t gp_input_keys(uint32_t,uint32_t);
uint32_t np_rom_size(const volatile uint8_t*,uint32_t);
uint32_t np_audio_samples(void);
uint32_t np_profile_read(uint32_t);
void np_render_stride(uint32_t);
uint32_t np_video_frame(void);
const uint32_t *np_palette(void);
#ifdef __cplusplus
}
#endif

