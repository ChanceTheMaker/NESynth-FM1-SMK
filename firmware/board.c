/* SPDX-License-Identifier: GPL-3.0-only
 * FM-1 and SMK37 adapters using MusicCoach/Felucca HAL. Compile-only candidate.
 * Audio IRQ drains PCM; CPU0 emulates NES/APU; CPU1 scales immutable display frames. */
#include <stdint.h>
#include "gb_port.h"
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "smk37_bootguard.h"
#include "cartridge.h"

extern void gb_isr_audio(void),gb_isr_scan(void);
static int32_t dma[256*4] __attribute__((aligned(4)));
static int16_t samples[256*2];
static uint16_t screen[240*225] __attribute__((section(".pool"),aligned(4)));
static volatile uint32_t scan_count,audio_count,key_lo,key_hi,audio_underruns;
static uint32_t display_count;
static uint32_t video_dirty_top,video_dirty_bottom=225,display_first=1;
static uint32_t emu_last_ticks,emu_max_ticks,emu_total_ticks,emu_measured_frames;
static uint32_t irq_total_ticks;
static uint32_t lcd_started,lcd_max_ticks,lcd_measuring;
static uint32_t input_buttons,input_seen;
static volatile uint32_t splash_active;
static void read_samples(void){gp_audio_read(samples,256);}
#include "multicore_video.h"
volatile smk37_bootguard_t gb_bootguard __attribute__((section(".noinit")));

#if GB_SMK37
#include "smk37_boot.h"
#include "smk37_time.h"
#include "smk37_audio.h"
#include "smk37_lcd_hw.h"
#include "smk37_panel.h"
#include "smk37_scanner.h"
#include "smk37_contacts.h"
#include "smk37_keyscan.h"
#include "smk37_controls.h"
#include "smk37_adc.h"
#include "smk37_lights_hw.h"
#include "smk37_game_controls.h"
#include "smk37_game_pads.h"
static smk37_keyscan_t keys[37];
static smk37_adc_t pad_adc;
static smk37_controls_t panel_controls;
static gb_smk37_pads_t game_pads;
static volatile uint32_t smk_buttons;
static uint8_t pad_lights[SMK37_LIGHT_BYTES] __attribute__((aligned(4)));
static int lcd_command(uint8_t c){return smk37_lcd_command(c,4000000);}
static int lcd_data(const void*p,uint32_t n){return smk37_lcd_data(p,n,4000000);}
static const smk37_panel_io_t panel={lcd_command,lcd_data,fm1_delay_ms};
static const smk37_audio_hooks_t audio_hooks={fm1_delay_ms,fm1_irq_attach};
void gb_scan_irq(void) {
    if(!(SMK37_SCAN_CON&0x8000u))return;
    unsigned index=pad_adc.scan.index;
    int result=smk37_adc_spi_done(&pad_adc);
    if(result>0)for(unsigned p=0;p<16;p++)if(index==smk37_pad_input[p])
        gb_smk37_pad_sample(&game_pads,p,pad_adc.scan.raw[index]);
    if(result<0)game_pads.held=game_pads.pressed=0;
    smk37_scan_set_analog(smk37_analog_mask(&pad_adc.scan));
    if(!smk37_scan_tick())return;
    (void)smk37_controls_tick(&panel_controls,smk37_scan_frame);
    smk_buttons=panel_controls.buttons;
    scan_count++;
    for(unsigned k=0;k<37;k++) {
        unsigned raw=smk37_contact_closed(smk37_scan_frame,smk37_key_contacts[k][0])|
            smk37_contact_closed(smk37_scan_frame,smk37_key_contacts[k][1])<<1;
        uint32_t duration;
        if(!smk37_keyscan_tick(&keys[k],raw,&duration))continue;
        if(k<32) {if(duration)key_lo|=1u<<k;else key_lo&=~(1u<<k);}
        else {if(duration)key_hi|=1u<<(k-32);else key_hi&=~(1u<<(k-32));}
    }
}
void gb_audio_irq(void) {
    uint8_t p=smk37_audio_pending();smk37_audio_ack_aux(p);
    if(!(p&SMK37_AUDIO_HALF))return;
    unsigned h=smk37_audio_free_half();read_samples();
    for(unsigned i=0;i<512;i++)dma[h*512+i]=(int32_t)samples[i]*256;
    audio_count++;smk37_audio_ack_half();
}
static int display_init(void){smk37_lcd_hw_init(4);return smk37_panel_init(&panel);}
static void display_frame(void){
    unsigned first=video_dirty_top,rows=video_dirty_bottom-first;
    if(smk37_panel_window(&panel,0,7+first,240,rows))lcd_data(screen+first*240,rows*480);
}
static int display_ready(void){return 1;}
static void input_init(void){
    smk37_adc_init(&pad_adc);
    SMK37_LIGHT_REG(0x50080u)&=~(1u<<10);
    SMK37_LIGHT_REG(0x50088u)&=~(1u<<10);
    smk37_scan_set_analog(smk37_analog_mask(&pad_adc.scan));
    smk37_scan_init(fm1_irq_attach,gb_isr_scan);
    uint32_t rgb[16]={0};
    for(unsigned p=0;p<16;p++)if(gp_smk37_pad_lights()&(1u<<p))
        rgb[p]=p>=14?0x060006u:0x000406u;
    smk37_light_encode_rgb(pad_lights,rgb);
    (void)smk37_lights_send(pad_lights,4000000);
}
static uint32_t smk_input_frame(void){
    fm1_irq_off();
    uint32_t lo=key_lo,hi=key_hi,pads=gb_smk37_pads_take(&game_pads),buttons=smk_buttons;
    fm1_irq_on();
    return gp_input_keys(lo,hi)|gp_smk37_pads(pads)|gp_smk37_panel(buttons);
}
static void audio_start(void){(void)smk37_audio_init(dma,512,gb_isr_audio,3,&audio_hooks);}
static void audio_stop(void){smk37_audio_stop();}
#else
#include "fm1_timer.h"
#include "fm1_input.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "master_volume.h"
#include "fm1_lcd_hw.h"
#include "../vendor/felucca/firmware/src/lcd.c"
static volatile uint32_t master_gain;
static int32_t master_raw=-1;
static uint32_t master_audio_gain;
static void master_boot(void){
    fm1_adc_init();(void)fm1_adc_read(FM1_ADC_MASTER);
    for(unsigned i=0;i<3;i++){
        int32_t raw=fm1_adc_read(FM1_ADC_MASTER);
        if(raw>=0){master_raw=raw;master_gain=master_audio_gain=gb_master_gain((uint32_t)raw);return;}
    }
    /* ADC failure starts muted; a later successful conversion recovers. */
    master_gain=master_audio_gain=0;
}
static void master_poll(void){
    static uint32_t last;
    uint32_t now=fm1_ticks();if(now-last<240000u)return;last=now;
    int32_t raw=fm1_adc_read(FM1_ADC_MASTER);
    master_raw=raw;if(raw>=0)master_gain=gb_master_gain((uint32_t)raw);
}
static void recovery_tick(void);
void gb_scan_irq(void){uint32_t begun=fm1_ticks();fm1_timer5_ack();fm1_input_tick();key_lo=fm1_in.notes;scan_count++;recovery_tick();irq_total_ticks+=fm1_ticks()-begun;}
void gb_audio_irq(void){
    uint32_t p=fm1_audio_pending();fm1_audio_ack_aux(p);
    if(!(p&FM1_AUDIO_HALF))return;
    unsigned h=fm1_audio_free_half();if(!splash_active&&gp_audio_queued()<256)audio_underruns++;
    read_samples();
    int32_t start=(int32_t)master_audio_gain,target=(int32_t)master_gain;
    for(unsigned i=0;i<256;i++){
        uint32_t gain=(uint32_t)(start+(target-start)*(int32_t)(i+1)/256);
        dma[h*512+i*2]=gb_master_sample(samples[i*2],gain);
        dma[h*512+i*2+1]=gb_master_sample(samples[i*2+1],gain);
    }
    master_audio_gain=(uint32_t)target;
    audio_count++;fm1_audio_ack_half();
}
static int display_init(void){lcd_init();return 1;}
/* Transfer asynchronously so enlarging the image cannot block emulation for
   the duration of a full LCD DMA. Never overwrite a buffer still in flight. */
static int display_ready(void){
    if(lcd_busy&&!(FM1_LCD_SPI_CON&0x8000u))return 0;
    lcd_sync();
    if(lcd_measuring){uint32_t dt=fm1_ticks()-lcd_started;if(dt>lcd_max_ticks)lcd_max_ticks=dt;lcd_measuring=0;}
    return 1;
}
static void display_frame(void){
    lcd_started=fm1_ticks();lcd_measuring=1;
    lcd_blit(0,7+video_dirty_top,240,video_dirty_bottom-video_dirty_top,screen+video_dirty_top*240);
}
#include "fm1_game_lights.h"
static void piano_lights(uint32_t held_actions){
    static uint32_t previous=UINT32_MAX;
    if(held_actions==previous)return;
    previous=held_actions;
    uint32_t lights=gp_fm1_key_lights(held_actions);
    for(unsigned k=0;k<27;k++)fm1_led_key(14+k,(lights&(1u<<k))!=0);
}
static void input_init(void){
    fm1_input_init();
    piano_lights(0);
    fm1_led_key(12,1); /* PLAY / Start */
    fm1_led_key(13,1); /* REC / Select */
    fm1_timer5_start(gb_isr_scan,4);
}
static void audio_start(void){fm1_audio_init(dma,512,gb_isr_audio,3);}
static void audio_stop(void){fm1_audio_stop();}
#include "fm1_recovery.h"
#endif
static void fm1_fault(const fm1_crash_t *c){(void)c;video_retire();audio_stop();fm1_reboot();}
extern uint32_t _data_start[],_data_end[],_data_load[],_bss_start[],_bss_end[];
extern uint32_t _pool_start[],_pool_end[],_rt_start[],_rt_end[],_rt_load[];
extern uint32_t _dsp_start[],_dsp_end[],_dsp_load[];
static void copy(uint32_t*d,uint32_t*end,const uint32_t*s){while(d<end)*d++=*s++;}
static void zero(uint32_t*d,uint32_t*end){while(d<end)*d++=0;}
void gb_cstart(const uint32_t *parameters){
    fm1_core1_stop();fm1_p33_timeouts=0;fm1_reset_reason();
    uint32_t cold=fm1_boot.p3_rst&1u;
    fm1_wdt_arm(0x0d);
    if(smk37_bootguard_begin(&gb_bootguard,cold))fm1_enter_uboot();
#if GB_SMK37
    smk37_boot_capture(parameters);
#else
    (void)parameters;
#endif
    zero(_bss_start,_bss_end);zero(_pool_start,_pool_end);
    copy(_data_start,_data_end,_data_load);copy(_rt_start,_rt_end,_rt_load);
    copy(_dsp_start,_dsp_end,_dsp_load);__asm__ volatile("csync":::"memory");
#if GB_SMK37
    smk37_time_init();
#else
    fm1_time_init();
#endif
    fm1_irq_init();fm1_mailbox_clear();
    int display_ok=display_init();
    /* Volatile header read: the public template's ROM slot is patched in the
       browser, so the compiler must not fold its original header bytes. */
    const volatile uint8_t *cart_header=cartridge;
    uint32_t cart_size=np_rom_size(cart_header,sizeof cartridge);
    if(!cart_size||cart_size>sizeof cartridge||gp_load(cartridge,cart_size)) {
        /* Invalid embedded cartridge: allow bootguard/watchdog recovery. */
        for(;;);
    }
    input_init();
#if !GB_SMK37
    master_boot();
#endif
    splash_active=0;
    audio_start();fm1_irq_enable_all();
    gb_video_palette_init();
    np_render_stride(0); // Audio-first: adapt raster work to queued audio.
    for(unsigned i=0;i<4;i++)gp_frame();
    splash_active=0;
#if !GB_SMK37
    recovery_init();
#endif
    video_start(cold);
    uint32_t drawn=0,last=fm1_ticks(),elapsed=0,seconds=0;
    for(;;){
        uint32_t now=fm1_ticks();elapsed+=now-last;last=now;
        if(elapsed>=24000000){elapsed-=24000000;seconds++;}
        fm1_wdt_feed();
#if !GB_SMK37
        input_buttons=gp_input_keys(key_lo,key_hi);
        piano_lights(input_buttons);
        /* Measured FM-1 panel IDs: PLAY=12, REC=13. */
        if(fm1_in.buttons&(1u<<12))input_buttons|=GP_START;
        if(fm1_in.buttons&(1u<<13))input_buttons|=GP_SELECT;
        input_seen|=input_buttons;gp_buttons(input_buttons);
        master_poll();
        recovery_service();
#endif
        if(gp_audio_queued()<2955){
#if GB_SMK37
            input_buttons=smk_input_frame();input_seen|=input_buttons;gp_buttons(input_buttons);
#endif
            uint32_t begun=fm1_ticks();
            int advanced=gp_frame();
            emu_last_ticks=fm1_ticks()-begun;
            if(advanced){emu_measured_frames++;emu_total_ticks+=emu_last_ticks;
                if(emu_last_ticks>emu_max_ticks)emu_max_ticks=emu_last_ticks;}
        }
        if(display_ok && video_complete()){
            if(display_first){video_dirty_top=0;video_dirty_bottom=225;display_first=0;}
            if(video_dirty_bottom>video_dirty_top){display_frame();display_count++;}
        }
        if(display_ok && !video_pending && np_video_frame()!=drawn && display_ready()){
            video_submit(gp_pixels());drawn=np_video_frame();
            if(!video_online){
                if(display_first){video_dirty_top=0;video_dirty_bottom=225;display_first=0;}
                if(video_dirty_bottom>video_dirty_top){display_frame();display_count++;}
            }
        }
        if(seconds>=30 && scan_count>100 && audio_count>100 && gp_frame_count()>100)
            smk37_bootguard_healthy(&gb_bootguard);
        if(gp_error()) {audio_stop();fm1_reboot();}
    }
}
