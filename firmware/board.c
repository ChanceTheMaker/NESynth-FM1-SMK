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
static volatile uint32_t irq_total_ticks;
static uint32_t lcd_started,lcd_max_ticks,lcd_measuring;
static uint32_t input_buttons,input_seen;
static volatile uint32_t splash_active;
static void read_samples(void){gp_audio_read(samples,256);}
#include "multicore_video.h"
volatile smk37_bootguard_t gb_bootguard __attribute__((section(".noinit")));

#if GB_SMK37
/* The scanner also includes the LED encoder: configure it before either HAL. */
#define SMK37_LIGHT_MAX_LEVEL 255u /* Readable game pads; diagnostics keep their dim default. */
#include "smk37_boot.h"
#include "smk37_time.h"
#include "smk37_audio.h"
#include "smk37_lcd_hw.h"
#define SMK_LCD_DMA_BARRIER() __asm__ volatile("csync" ::: "memory")
#include "smk37_display_dma.h"
static smk_display_dma_t display_dma;
#include "smk37_panel.h"
#include "smk37_scan_config.h"
#include "smk37_scanner.h"
#include "smk37_contacts.h"
#include "smk37_keyscan.h"
#include "smk37_controls.h"
#include "smk37_adc.h"
#define SMK37_LIGHT_RESET() fm1_delay_us(350)
#define SMK37_LIGHT_DMA_BARRIER() __asm__ volatile("csync" ::: "memory")
#include "smk37_lights_hw.h"
_Static_assert(SMK37_LIGHT_COMPILED_MAX_LEVEL==255u,"SMK pad encoder must use application brightness");
#include "smk37_game_controls.h"
#include "smk37_game_pads.h"
#include "smk37_volume.h"
#include "smk37_brightness.h"
static smk_brightness_t smk_brightness_fader;
static volatile uint32_t smk_brightness=150;
static volatile uint32_t smk_volume_target=64;
static uint32_t smk_volume_current=64; /* Same 25% startup level as before. */
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
    uint32_t begun=fm1_ticks();
    if(!(SMK37_SCAN_CON&0x8000u))return;
    unsigned index=pad_adc.scan.index;
    int result=smk37_adc_spi_done(&pad_adc);
    if(result>0)smk_volume_target=smk_volume_sample(smk_volume_target,index,pad_adc.scan.raw[index]);
    if(result>0)smk_brightness=smk_brightness_sample(&smk_brightness_fader,smk_brightness,index,pad_adc.scan.raw[index]);
    if(result>0)for(unsigned p=0;p<16;p++)if(index==smk37_pad_input[p])
        gb_smk37_pad_sample(&game_pads,p,pad_adc.scan.raw[index]);
    if(result<0)game_pads.held=game_pads.pressed=0;
    smk37_scan_set_analog(smk37_analog_mask(&pad_adc.scan));
    if(!smk37_scan_tick()){irq_total_ticks+=fm1_ticks()-begun;return;}
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
    irq_total_ticks+=fm1_ticks()-begun;
}
void gb_audio_irq(void) {
    uint8_t p=smk37_audio_pending();smk37_audio_ack_aux(p);
    if(!(p&SMK37_AUDIO_HALF))return;
    unsigned h=smk37_audio_free_half();if(!splash_active&&gp_audio_queued()<256)audio_underruns++;
    read_samples();
    uint32_t target=smk_volume_target;
    smk_volume_render(dma+h*512,samples,smk_volume_current,target);
    smk_volume_current=target;
    audio_count++;smk37_audio_ack_half();
}
static int display_init(void){smk37_lcd_hw_init(4);return smk37_panel_init(&panel);}
static void display_frame(void){
    if(display_dma.busy||display_dma.failed)return;
    unsigned first=video_dirty_top,rows=video_dirty_bottom-first;
    if(!smk37_panel_window(&panel,0,7+first,240,rows)||
       !smk_display_begin(&display_dma,screen+first*240,rows*480,fm1_ticks())){
        display_dma.failed=1;smk_display_stop(&display_dma);
    }
}
static int display_ready(void){
    int ready=smk_display_poll(&display_dma,fm1_ticks());lcd_max_ticks=display_dma.max_ticks;
    return ready>0;
}
static void display_stop(void){smk_display_stop(&display_dma);}
/* Main-loop only: leave the shared SPI peripheral to any active LCD DMA. */
static void smk_pad_lights(uint32_t held){
    static uint32_t previous=UINT32_MAX,last;
    uint32_t lit=gp_smk37_pad_lights()&~held,level=smk_brightness;
    uint32_t state=lit|(level<<16);
    if(state==previous||!display_ready())return;
    uint32_t now=fm1_ticks();
    /* Coalesce fader changes to 50 Hz; pad press/release feedback stays immediate. */
    if(lit==(previous&0xffffu)&&(uint32_t)(now-last)<480000u)return;
    uint32_t rgb[16]={0};
    for(unsigned p=0;p<16;p++)if(lit&(1u<<p))
        rgb[p]=p>=14?(level<<16)|level:((level*2/3)<<8)|level;
    smk37_light_encode_rgb(pad_lights,rgb);
    if(smk37_lights_send(pad_lights,4000000)){previous=state;last=now;}
}
static void input_init(void){
    smk37_adc_init(&pad_adc);
    SMK37_LIGHT_REG(0x50080u)&=~(1u<<10);
    SMK37_LIGHT_REG(0x50088u)&=~(1u<<10);
    smk37_scan_set_analog(smk37_analog_mask(&pad_adc.scan));
    /* Transport LEDs: PLAY green (output 17), record red (output 19).
       Output 18 is PLAY's alternate red LED, not the REC input ID. */
    smk37_scan_indicator_bits=(1u<<16)|(1u<<18);
    smk37_scan_init(fm1_irq_attach,gb_isr_scan);
    smk_pad_lights(0);
}
static uint32_t smk_input_frame(void){
    fm1_irq_off();
    uint32_t lo=key_lo,hi=key_hi,pads=gb_smk37_pads_take(&game_pads),buttons=smk_buttons;
    uint32_t held=game_pads.held;
    fm1_irq_on();
    smk_pad_lights(held);
    return gp_input_keys(lo,hi)|gp_smk37_pads(pads)|gp_smk37_panel(buttons);
}
static void audio_start(void){(void)smk37_audio_init(dma,512,gb_isr_audio,3,&audio_hooks);}
static void audio_stop(void){smk37_audio_stop();}
#include "smk37_recovery.h"
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
    fm1_led_key(12,1); /* PLAY / NES Select (left) */
    fm1_led_key(13,1); /* REC / NES Start (right) */
    fm1_timer5_start(gb_isr_scan,4);
}
static void audio_start(void){fm1_audio_init(dma,512,gb_isr_audio,3);}
static void audio_stop(void){fm1_audio_stop();}
#include "fm1_recovery.h"
#endif
static void fm1_fault(const fm1_crash_t *c){(void)c;
#if GB_SMK37
    display_stop();
#endif
    video_retire();audio_stop();fm1_reboot();}
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
#if GB_SMK37
    gp_volume(100); /* Fader 1 applies master gain at the audio DMA boundary. */
#else
    master_boot();
#endif
    splash_active=0;
    audio_start();fm1_irq_enable_all();
    gb_video_palette_init();
    np_render_stride(0); // Audio-first: adapt raster work to queued audio.
    for(unsigned i=0;i<4;i++)gp_frame();
    splash_active=0;
    recovery_init();
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
        if(fm1_in.buttons&(1u<<12))input_buttons|=GP_SELECT;
        if(fm1_in.buttons&(1u<<13))input_buttons|=GP_START;
        input_seen|=input_buttons;gp_buttons(input_buttons);
        master_poll();
#endif
        recovery_service();
#if GB_SMK37
        /* Release the framebuffer only when the peripheral is done. This
         * leaves CPU0 free for NES/audio while the large pixel DMA runs. */
        if(display_ok)(void)display_ready();
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
