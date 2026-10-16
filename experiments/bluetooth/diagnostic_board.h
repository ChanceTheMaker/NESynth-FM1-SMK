/* SPDX-License-Identifier: GPL-3.0-only */
/* Included only in the generated SMK diagnostic board translation unit. */
#include "fm1_guard.h"
#include "fm1_ble_hal.h"
#define FL_RANGE_OK(off,n) 0 /* Application flash erase/program always refuse. */
#include "fm1_flash.h"
extern void np_ble_diag_start(void),np_ble_diag_service(void);
extern uint32_t np_ble_diag_state(void),np_ble_diag_error(void),np_ble_diag_writes(void);
uint32_t core_ms(void){return ota_now_ms();}
void core_wdt_feed(void){fm1_wdt_feed();}
void core_reboot(void){fm1_reboot();}
void core_audio_stop(void){audio_stop();}
void core_irq_attach(uint32_t n,void(*h)(void),uint32_t priority){
    if(n>=128){fm1_reboot();return;}
    fm1_irq_attach(n,h,priority);
}
void core_irq_mask(uint32_t n){if(n<128)fm1_irq_mask(n);}
void core_irq_unmask(uint32_t n){if(n<128)fm1_irq_unmask(n);}
uint32_t core_irq_is_masked(uint32_t n){return n<128?fm1_irq_is_masked(n):1;}
void core_ble_stack_window(void){fm1_guard_stack_window((uint32_t)(uintptr_t)video_source,(uint32_t)(uintptr_t)_sstack_top);}
int core_flash_read(uint32_t off,void *p,uint32_t n){
    /* Before radio only; CPU1 never starts and display DMA must be idle. */
    if(np_ble_diag_state()||display_dma.busy||off<0x9d000u||off>=0xbd000u||!n||n>0xbd000u-off)return -1;
    fm1_irq_off();int result=FL_FAR(fl_read_ram)(off,p,n);fm1_irq_on();return result;
}
uint32_t core_audio_late(void){return 0;}
uint32_t core_ui_frames(void){return display_count;}
void core_con_puts(const char *s){(void)s;}
void core_midi_out(uint32_t p){(void)p;}
void core_ble_midi_in(uint32_t p){(void)p;}
void core_ble_link(int up){(void)up;}
uint32_t core_ble_in_drops(void){return 0;}
/* Original compact 3x5 glyphs, enlarged 3x. Rows are three-bit values. */
static const uint8_t np_font[][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {2,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},
 {7,4,6,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},
 {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},
 {7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},{7,4,7,1,7},{7,2,2,2,2},
 {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7}
};
static void np_text(unsigned y,const char *s){
    for(unsigned x=12;*s&&x+9<240;s++,x+=12){
        unsigned ch=(unsigned char)*s,idx=ch>='0'&&ch<='9'?ch-'0':ch>='A'&&ch<='Z'?ch-'A'+10:99;
        if(idx>=36)continue;
        for(unsigned row=0;row<5;row++)for(unsigned col=0;col<3;col++)if(np_font[idx][row]&(4u>>col))
            for(unsigned dy=0;dy<3;dy++)for(unsigned dx=0;dx<3;dx++)screen[(y+row*3+dy)*240+x+col*3+dx]=0xffff;
    }
}
static void np_number(unsigned y,uint32_t n){
    char text[11];unsigned count=0;do{text[count++]=(char)('0'+n%10);n/=10;}while(n&&count<10);
    for(unsigned i=0;i<count/2;i++){char t=text[i];text[i]=text[count-1-i];text[count-1-i]=t;}
    text[count]=0;np_text(y,text);
}
static __attribute__((noreturn)) void np_diag_run(void){
    uint32_t previous=1,shown=~0u,last_draw=0,start_pending=0; /* require a fresh PLAY press */
    for(;;){
        fm1_wdt_feed();uint32_t now=core_ms();
        /* The RAM updater may only run with the controller never started.
         * An update request after START reboots to radio-off mode; retry then. */
        if(np_ble_diag_state()&&usb.ota_req){usb.ota_req=0;fm1_reboot();}
        recovery_service();
        uint32_t play=(smk_buttons>>16)&1u;
        if(play&&!previous&&!np_ble_diag_state())start_pending=1;
        if(start_pending&&display_ready()){start_pending=0;np_ble_diag_start();}
        previous=play;np_ble_diag_service();
        if(now>30000&&scan_count>100)smk37_bootguard_healthy(&gb_bootguard);
        uint32_t state=np_ble_diag_state();
        if((state!=shown||now-last_draw>=500)&&display_ready()){
            for(unsigned i=0;i<240*225;i++)screen[i]=0x0821;
            np_text(12,"NESYNTH BLE TEST");
            const char *labels[]={"PRESS PLAY","STARTING","RADIO READY","ADVERTISING","CONNECTED","RADIO ERROR"};
            np_text(48,labels[state<6?state:5]);
            np_text(84,"ECHO COUNT");np_number(108,np_ble_diag_writes());
            np_text(142,"ERROR");np_number(164,np_ble_diag_error());
            np_text(198,state?"REBOOT FOR USB":"USB RESTORE READY");
            video_dirty_top=0;video_dirty_bottom=225;display_frame();display_count++;shown=state;last_draw=now;
        }
    }
}
