/* SPDX-License-Identifier: GPL-3.0-only
 * NES adapter derived from GameDude for Felucca's existing FM-1 USB updater.
 * Application writes are restricted to the existing OTA staging allocation. */
#include <string.h>
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_ID "FM-1_961"
#define RING_PUBLISH() __asm__ volatile("csync" ::: "memory")
#define FL_RANGE_OK(off,n) FL_IN(off,n,0xe0000u,0xe5000u)
#include "fm1_flash.h"
#include "../vendor/felucca/firmware/src/usb.c"
#include "../vendor/felucca/firmware/src/ota.c"
static uint8_t recovery_flash_ok;
static uint32_t ota_now_ms(void){return fm1_ms;}
static void ota_idle(void){fm1_wdt_feed();}
static int ota_erase(uint32_t off){
    uint32_t took;
    if(!recovery_flash_ok||!FL_RANGE_OK(off,0x1000u)||(off&0xfffu))return -8;
    return fl_erase4k(off,&took);
}
static int ota_prog(uint32_t off,const void *p,uint32_t n){
    if(!recovery_flash_ok||!FL_RANGE_OK(off,n))return -8;
    return fl_write(off,p,n);
}
static int ota_fread(uint32_t off,void *p,uint32_t n){
    if(off>0x100000u||n>0x100000u-off)return -8;
    uint32_t f=irq_save();int rc=FL_FAR(fl_read_ram)(off,p,n);irq_restore(f);return rc;
}
static void ota_show(uint32_t step,int32_t code){
    (void)code;
    /* Green progress bands; red on an update failure. */
    lcd_fill(0,0,240,240,step==9?0xf800:0);
    if(step<6)lcd_fill(20,100,step*40,24,0x07e0);
}
static void ota_commit(const uint8_t *parm){
    video_stop();gb_bootguard.pending=0;usb_detach();fm1_delay_ms(30);fm1_enter_update(parm);
}
static void recovery_tick(void){
    static uint32_t last,acc,sub;
    uint32_t now=fm1_ticks();acc+=now-last;last=now;
    fm1_ms+=acc/24000u;acc%=24000u;
    if(++sub==5){sub=0;usb_poll();}
}
static void recovery_init(void){
    uint32_t f=irq_save();recovery_flash_ok=FL_FAR(fl_jedec_ram)()==0x856014u;irq_restore(f);
    /* Only stale OTA loader/record sectors; never presets, user data or calibration. */
    if(recovery_flash_ok)ota_boot_cleanup();
    usb_start();
}
static void recovery_service(void){
    usb_retry(fm1_ms);
    /* Private non-commercial diagnostics: 7D 'N' 'S', versions 1..4. Kept
       separate from all existing Felucca command IDs. Read-only. */
    const uint8_t *request;uint32_t request_size;
    if(ota_frame_get(&request,&request_size)&&request_size==4&&
       request[0]==0x7d&&request[1]=='N'&&request[2]=='S'&&(request[3]>=1&&request[3]<=4)){
        uint8_t version=request[3];ota_frame_done();
        uint32_t values[]={fm1_ms,gp_frame_count(),gp_audio_queued(),audio_count,scan_count,
            (uint32_t)master_raw,master_gain,key_lo,gp_error(),FM1_ALNK_CON0,FM1_ALNK_CON2,
            display_count,audio_underruns,fm1_in.buttons,input_buttons,input_seen,
            video_online,video_jobs,video_fallbacks,fm1_core1_fault.count,video_guard.barred,
            video_stage,video_core,video_launch_con,video_launch_pc,video_launch_emu,video_launch_dbg,
            fm1_core1_fault.pc,fm1_core1_fault.emu,fm1_core1_fault.dbg,
            video_launch_ready,video_retire_reason,video_retire_ticks,video_worker_jobs,video_work_max_ticks,
            emu_last_ticks,emu_max_ticks,emu_total_ticks,emu_measured_frames,lcd_max_ticks,
            np_profile_read(0),np_profile_read(1),np_profile_read(2),irq_total_ticks};
        uint8_t response[256]={0xf0,0x7d,'N','S',version};unsigned n=5;
        for(unsigned i=0;i<(version==1?16:version==2?21:version==3?40:sizeof values/sizeof values[0]);i++)
            for(unsigned j=0;j<5;j++)response[n++]=(uint8_t)((values[i]>>(j*7))&127);
        response[n++]=0xf7;ota_wire_send(response,n);
    }else ota_service();
    /* This emulator does not consume musical MIDI events. Keep that ring drained. */
    uint32_t f=irq_save();mi_r=mi_w;midi_in_overflow=0;irq_restore(f);
    if(usb.uboot_req){video_stop();audio_stop();gb_bootguard.pending=0;usb_detach();fm1_delay_ms(30);fm1_enter_uboot();}
    if(usb.ota_req){
        usb.ota_req=0;
        if(recovery_flash_ok){video_stop();audio_stop();ota_session();audio_start();}
    }
}
