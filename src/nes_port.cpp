/* SPDX-License-Identifier: GPL-3.0-only */
#include "nes_port.h"
#include "InfoNES.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"
#include "smk37_game_controls.h"
#include <string.h>
#if defined(GB_HARDWARE)
#define BARRIER() __asm__ volatile("csync" ::: "memory")
#else
#define BARRIER() __asm__ volatile("" ::: "memory")
#endif
static uint8_t pixels[256*240] NES_POOL;
static int16_t ring[4096] NES_POOL;
static volatile uint32_t read_pos,write_pos;
static uint32_t ready,error,paused,frames,buttons,volume=25,sample_acc,total_samples;
static uint32_t save_hash;
static int32_t previous_mix,highpass;
static const uint8_t *cart;
static uint32_t cart_size;
WORD NesPalette[64];
unsigned np_profile_totals[3];
unsigned np_skip_pixels;
static unsigned render_stride=1;
static uint32_t video_frame;
uint32_t np_video_frame(void){return video_frame;}
void np_render_stride(uint32_t stride){render_stride=stride<=6?stride:1;}
uint32_t np_profile_read(uint32_t index){return index<3?np_profile_totals[index]:0;}
extern unsigned int ApuSamplesPerSync;
extern BYTE np_prg_ram_enabled;
extern BYTE np_prg_ram_writable,np_mapper_irq_pending;
/* Classic illustrative NES palette. RGB values are shared by browser and LCD. */
static const uint32_t colors[64]={
0x666666,0x002a88,0x1412a7,0x3b00a4,0x5c007e,0x6e0040,0x6c0600,0x561d00,
0x333500,0x0b4800,0x005200,0x004f08,0x00404d,0,0,0,
0xadadad,0x155fd9,0x4240ff,0x7527fe,0xa01acc,0xb71e7b,0xb53120,0x994e00,
0x6b6d00,0x388700,0x0c9300,0x008f32,0x007c8d,0,0,0,
0xfffeff,0x64b0ff,0x9290ff,0xc676ff,0xf36aff,0xfe6ecc,0xfe8170,0xea9e22,
0xbcbe00,0x88d800,0x5ce430,0x45e082,0x48cdde,0x4f4f4f,0,0,
0xfffeff,0xc0dfff,0xd3d2ff,0xe8c8ff,0xfbc2ff,0xfec4ea,0xfeccc5,0xf7d8a5,
0xe4e594,0xcfef96,0xbdf4ab,0xb3f3cc,0xb5ebf2,0xb8b8b8,0,0};
const uint32_t *np_palette(void){return colors;}
uint32_t np_rom_size(const volatile uint8_t *r,uint32_t capacity){
    if(!r||capacity<16||r[0]!='N'||r[1]!='E'||r[2]!='S'||r[3]!=0x1a)return 0;
    uint32_t n=16u+((r[6]&4)?512u:0u)+r[4]*16384u+r[5]*8192u;
    return n<=capacity?n:0;
}
static uint32_t hash_save(){uint32_t h=2166136261u;for(unsigned i=0;i<8192;i++)h=(h^SRAM[i])*16777619u;return h;}
int np_validate_rom(const uint8_t *r,uint32_t n){
    if(!r||n<16)return GP_SHORT;
    if(r[0]!='N'||r[1]!='E'||r[2]!='S'||r[3]!=0x1a)return GP_FORMAT;
    if((r[7]&15)!=0)return GP_FORMAT; // iNES 1.0, standard NES only
    unsigned mapper=(r[6]>>4)|(r[7]&0xf0);
    if(mapper>4&&mapper!=7&&mapper!=9&&mapper!=11)return GP_MAPPER;
    if(r[9]&1)return GP_REGION;
    if(r[8]>1)return GP_RAM_SIZE;
    for(unsigned i=10;i<16;i++)if(r[i])return GP_FORMAT;
    if(np_rom_size(r,n)!=n)return GP_ROM_SIZE;
    if(mapper==0&&((r[4]!=1&&r[4]!=2)||r[5]>1))return GP_ROM_SIZE;
    if(mapper==2&&(r[4]<2||r[4]>16||(r[4]&(r[4]-1))||r[5]||(r[6]&8)))return GP_ROM_SIZE;
    if(mapper==7&&(r[4]<2||r[4]>16||(r[4]&(r[4]-1))||r[5]||(r[6]&8)))return GP_ROM_SIZE;
    if(mapper==3&&((r[4]!=1&&r[4]!=2)||!r[5]||r[5]>4||(r[5]&(r[5]-1))||(r[6]&8)))return GP_ROM_SIZE;
    if(mapper==4&&(r[4]<2||r[4]>32||(r[4]&(r[4]-1))||r[5]>32||(r[5]&&(r[5]&(r[5]-1)))))return GP_ROM_SIZE;
    if(mapper==11&&(r[4]<2||r[4]>8||(r[4]&(r[4]-1))||!r[5]||r[5]>16||(r[5]&(r[5]-1))||(r[6]&8)))return GP_ROM_SIZE;
    if(mapper==1){
        if(!r[4]||r[4]>16||(r[4]&(r[4]-1))||r[5]>16||(r[5]&&(r[5]&(r[5]-1)))||(r[6]&8))return GP_ROM_SIZE;
    }
    // MMC2: up to 128 KiB PRG + 128 KiB CHR ROM; no CHR RAM/four-screen boards.
    if(mapper==9){
        if(r[4]<2||r[4]>8||(r[4]&(r[4]-1))||!r[5]||r[5]>16||(r[5]&(r[5]-1)))return GP_ROM_SIZE;
        if(r[6]&8)return GP_FORMAT;
    }
    return 0;
}
int gp_load(const uint8_t *r,uint32_t n){
    ready=0;error=0;paused=0;frames=0;write_pos=read_pos=0;buttons=0;cart=0;np_skip_pixels=0;video_frame=0;
    if((error=np_validate_rom(r,n)))return error;
    cart=r;cart_size=n;memcpy(&NesHeader,r,16);
    memset(SRAM,0,8192);if(r[6]&4)memcpy(SRAM+0x1000,r+16,512);
    ROM=(BYTE*)(r+16+((r[6]&4)?512:0));VROM=ROM+r[4]*16384u;
    for(unsigned i=0;i<64;i++)NesPalette[i]=(WORD)i;
    memset(pixels,0,sizeof pixels);sample_acc=total_samples=0;previous_mix=highpass=0;
    np_prg_ram_enabled=np_prg_ram_writable=1;np_mapper_irq_pending=0;
    InfoNES_Init();if(InfoNES_Reset())return error=GP_MAPPER;
    PPU_UpDown_Clip=0;APU_Mute=0;FrameSkip=0;save_hash=hash_save();ready=1;return 0;
}
int gp_frame(void){
    if(!ready||error||paused||4096-gp_audio_queued()<735)return 0;
    if(render_stride)np_skip_pixels=frames%render_stride!=0;
    else {
        // Audio-first hardware mode: 10–20 raster frames/sec at full game speed.
        unsigned since=frames+1-video_frame;
        np_skip_pixels=video_frame && since<6 && (since<3 || gp_audio_queued()<2200);
    }
    sample_acc+=44100u*59561u;
    ApuSamplesPerSync=sample_acc/3579545u;sample_acc%=3579545u;
    uint32_t before=write_pos;
    InfoNES_Cycle();
    if(write_pos==before&&!error)error=GP_TIMEOUT;
    if(error)return 0;
    frames++;if(!np_skip_pixels)video_frame=frames;return 1;
}
void gp_buttons(uint32_t p){if((p&48)==48)p&=~48u;if((p&192)==192)p&=~192u;buttons=p&255;}
void gp_pause(uint32_t p){paused=!!p;gp_buttons(0);}
void gp_reset(void){
    if(!ready)return;
    np_prg_ram_enabled=np_prg_ram_writable=1;np_mapper_irq_pending=0;
    InfoNES_Init();InfoNES_Reset();PPU_UpDown_Clip=0;APU_Mute=0;
    error=paused=frames=sample_acc=total_samples=0;read_pos=write_pos=0;
    np_skip_pixels=0;video_frame=0;
    previous_mix=highpass=0;memset(pixels,0,sizeof pixels);gp_buttons(0);
}
void gp_volume(uint32_t v){volume=v>100?100:v;}
uint8_t *gp_pixels(void){return pixels;}
uint8_t *gp_save_data(void){return SRAM;}
uint32_t gp_save_size(void){return ready&&(cart[6]&2)?8192:0;}
uint32_t gp_save_dirty(void){return gp_save_size()&&hash_save()!=save_hash;}
void gp_save_clean(void){save_hash=hash_save();}
uint32_t gp_audio_queued(void){uint32_t w=write_pos;BARRIER();return w-read_pos;}
uint32_t np_audio_samples(void){return total_samples;}
uint32_t gp_error(void){return error;}
uint32_t gp_frame_count(void){return frames;}
const char *gp_title(void){return ready&&MapperNo==7?"NES / AxROM":ready&&MapperNo==9?"NES / MMC2":ready&&MapperNo==4?"NES / MMC3":ready&&MapperNo==3?"NES / CNROM":ready&&MapperNo==1?"NES / MMC1":"NES / NROM";}
void gp_audio_read(int16_t *out,uint32_t n){
    uint32_t r=read_pos,w=write_pos;BARRIER();
    for(unsigned i=0;i<n;i++){
        int16_t v=0;if(r!=w){if(!paused&&!error)v=ring[r&4095];r++;}
        out[2*i]=out[2*i+1]=v;
    }
    BARRIER();read_pos=r;
}
void np_core_error(unsigned code){error=code;}
#if defined(GB_HARDWARE)
__attribute__((section(".dsp_text")))
#endif
static void capture_line(unsigned y,const WORD *p){
    if(np_skip_pixels)return;
    if(y>=240){error=GP_CPU;return;}
    for(unsigned x=0;x<256;x++)pixels[y*256+x]=(uint8_t)(p[x]&63);
}
void np_capture_line(unsigned y,const WORD *p){
    void (*volatile fn)(unsigned,const WORD*)=capture_line;fn(y,p);
}
void InfoNES_PadState(DWORD *p1,DWORD *p2,DWORD *system){
    *p1=(buttons&15)|((buttons&GP_UP)?16:0)|((buttons&GP_DOWN)?32:0)|
        ((buttons&GP_LEFT)?64:0)|((buttons&GP_RIGHT)?128:0);
    *p2=0;*system=PAD_SYS_QUIT; // Yield once per vblank; preserve CPU/PPU state.
}
void InfoNES_LoadFrame(){}
void InfoNES_SoundInit(){}
int InfoNES_SoundOpen(int,int rate){if(rate!=44100)error=GP_CPU;return 1;}
void InfoNES_SoundClose(){}
void InfoNES_SoundOutput(int n,BYTE *a,BYTE *b,BYTE *c,BYTE *d,BYTE *e){
    if(n<0||n>735||gp_audio_queued()+(uint32_t)n>4096){error=GP_TIMEOUT;return;}
    uint32_t w=write_pos;
    for(int i=0;i<n;i++){
        // Approximate linear mix, with a DC-blocking filter and conservative gain.
        int32_t mix=(a[i]+b[i]+c[i]+d[i]+e[i])*64;
        highpass=mix-previous_mix+(highpass*255)/256;previous_mix=mix;
        int32_t v=highpass*(int32_t)volume/100;
        if(v>32767)v=32767;if(v< -32768)v=-32768;
        ring[(w+i)&4095]=(int16_t)v;
    }
    BARRIER();write_pos=w+n;total_samples+=n;
}
void *InfoNES_MemoryCopy(void*d,const void*s,int n){return memcpy(d,s,(size_t)n);}
void *InfoNES_MemorySet(void*d,int v,int n){return memset(d,v,(size_t)n);}
void InfoNES_DebugPrint(char*){}
void InfoNES_Wait(){}
void InfoNES_MessageBox(char*,...){}
int InfoNES_Menu(){return -1;}
int InfoNES_ReadRom(const char*){return -1;}
void InfoNES_ReleaseRom(){}
uint32_t gp_input_keys(uint32_t lo,uint32_t hi){
#if GB_SMK37
    return gp_smk37_keys(lo,hi);
#else
    (void)hi;static const uint8_t keys[8]={26,24,11,12,7,0,3,4};uint32_t p=0;
    for(unsigned i=0;i<8;i++)if(lo&(1u<<keys[i]))p|=1u<<i;
    if(lo&(1u<<2))p|=GP_LEFT;if(lo&(1u<<6))p|=GP_RIGHT;if(lo&(1u<<5))p|=GP_UP;return p;
#endif
}

