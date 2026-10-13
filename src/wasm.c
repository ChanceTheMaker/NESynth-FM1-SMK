#include "nes_port.h"
static uint8_t upload[1024*1024];
static int16_t output[2048*2];
uint8_t *gp_upload(void){return upload;}
int gp_load_upload(uint32_t n){return n>sizeof upload?GP_ROM_SIZE:gp_load(upload,n);}
int16_t *gp_render_audio(uint32_t n){if(n>2048)n=2048;gp_audio_read(output,n);return output;}
