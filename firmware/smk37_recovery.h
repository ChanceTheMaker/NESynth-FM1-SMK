/* SPDX-License-Identifier: GPL-3.0-only
 * SMK RAM-loader handoff adapted from Doom's Felucca integration.
 * The game never erases or programs flash; the RAM-only loader owns writes.
 */
#include <string.h>
#include "fm1_timer.h"
#include "nes_loader_image.h"
#include "diagnostic_packet.h"
#define FELUCCA_OTA 1
#define FELUCCA_SYSEX 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define RING_PUBLISH() __asm__ volatile("csync" ::: "memory")
#include "../vendor/smk-updater/usb.c"
static uint8_t recovery_ready;
static volatile uint32_t usb_irq_ticks;
static uint32_t ota_now_ms(void){
    static uint32_t last,acc,ms;uint32_t now=fm1_ticks();acc+=now-last;last=now;
    ms+=acc/24000u;acc%=24000u;return ms;
}
static void ota_idle(void){fm1_wdt_feed();}
extern void gb_isr_usb(void);
void gb_usb_irq(void){uint32_t begun=fm1_ticks();fm1_timer5_ack();usb_poll();usb_irq_ticks+=fm1_ticks()-begun;}
static void recovery_init(void){
    usb_start();FM1_T5_CON=0x4000u;FM1_T5_CNT=0;FM1_T5_PRD=3000u;
    fm1_irq_attach(FM1_IRQ_TIMER5,gb_isr_usb,2);
    recovery_ready=1;FM1_T5_CON=0x4019u;
}
static void recovery_enter(void){
    display_stop();video_retire();audio_stop();
    fm1_irq_off();FM1_T5_CON=0x4000u;
    usb_detach();fm1_delay_ms(100);
    volatile uint8_t *dst=(volatile uint8_t*)0x01c0a800u;
    for(unsigned i=0;i<sizeof np_loader_image;i++)dst[i]=np_loader_image[i];
    __asm__ volatile("csync" ::: "memory");
    void (*volatile enter)(void)=(void(*)(void))0x01c0a800u;
    enter();for(;;);
}
static void recovery_service(void){
    if(!recovery_ready)return;
    uint32_t now_ms=ota_now_ms();
    if(!usb.up&&!usb.detached){
        FM1_T5_CON=0x4000u;usb_retry(now_ms);FM1_T5_CNT=0;FM1_T5_CON=0x4019u;
    }
    mi_r=mi_w;
    const uint8_t *p;uint32_t n;
    static const uint8_t query[]={0,0x32,0x45,0,0,0,0x40,0x7f};
    if(ota_frame_get(&p,&n)){
        int identity=n==sizeof query&&!memcmp(p,query,n);
        unsigned version=np_diagnostic_version(p,n);ota_frame_done();
        if(identity)ota_wire_send(np_identity_wire,sizeof np_identity_wire);
        else if(version){
            uint32_t values[NP_DIAGNOSTIC_FIELDS]={now_ms,gp_frame_count(),gp_audio_queued(),audio_count,scan_count,
                pad_adc.scan.raw[smk37_fader_input[0]],smk_volume_target,key_lo,gp_error(),SMK37_AUDIO_CON0,SMK37_AUDIO_CON2,
                display_count,audio_underruns,smk_buttons,input_buttons,input_seen,
                video_online,video_jobs,video_fallbacks,fm1_core1_fault.count,video_guard.barred,
                video_stage,video_core,video_launch_con,video_launch_pc,video_launch_emu,video_launch_dbg,
                fm1_core1_fault.pc,fm1_core1_fault.emu,fm1_core1_fault.dbg,
                video_launch_ready,video_retire_reason,video_retire_ticks,video_worker_jobs,video_work_max_ticks,
                emu_last_ticks,emu_max_ticks,emu_total_ticks,emu_measured_frames,lcd_max_ticks,
                np_profile_read(0),np_profile_read(1),np_profile_read(2),irq_total_ticks,
                display_dma.busy,display_dma.timeouts,display_dma.failed,usb_irq_ticks,
                *(volatile uint32_t*)0x10008u,*(volatile uint32_t*)0x10004u};
            uint8_t response[256];unsigned count=np_diagnostic_packet(response,sizeof response,version,values);
            ota_wire_send(response,count);
        }
    }
    if(usb.ota_req){usb.ota_req=0;gb_bootguard.pending=0;recovery_enter();}
    if(usb.uboot_req){display_stop();video_retire();audio_stop();gb_bootguard.pending=0;fm1_enter_uboot();}
}
