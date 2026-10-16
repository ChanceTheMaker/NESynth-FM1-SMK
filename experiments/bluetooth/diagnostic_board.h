/* SPDX-License-Identifier: GPL-3.0-only */
/* Included only in the generated SMK diagnostic board translation unit. */
#include "fm1_guard.h"
#include "fm1_ble_hal.h"
#ifndef FL_RANGE_OK
#define FL_RANGE_OK(off,n) 0 /* Application flash erase/program always refuse. */
#endif
#include "fm1_flash.h"
extern unsigned char *np_ble_probe_workspace(void);
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
void core_ble_stack_window(void){fm1_guard_stack_window((uint32_t)(uintptr_t)np_ble_probe_workspace(),(uint32_t)(uintptr_t)_sstack_top);}
static uint32_t np_read_count,np_read_off,np_read_size,np_read_sfc,np_read_magic[2];
static int np_read_result;
static uint32_t np_private_match;
int core_flash_read(uint32_t off,void *p,uint32_t n){
    np_read_count++;np_read_off=off;np_read_size=n;np_read_sfc=SFC_CON;
    /* Before radio only; CPU1 never starts and display DMA must be idle. */
    if(np_ble_diag_state()||display_dma.busy||off<0x9d000u||off>=0xbd000u||!n||n>0xbd000u-off){np_read_result=-1;return -1;}
    fm1_irq_off();int result=FL_FAR(fl_read_ram)(off,p,n);fm1_irq_on();
    np_read_result=result;
    if(!result&&n==4&&(off==0x9d000u||off==0xad000u)){
        const uint8_t *b=p;np_read_magic[off==0xad000u]=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
    }
    return result;
}
/* Private diagnostic: validate and exactly match the updater's usrtrim
 * identity record. Compare surrounding bytes only to explain layout changes.
 * No saved VM/music bytes are restored or modified. */
#include "private_calibration_guard.h"
static np_factory_report np_factory_check;
static int np_factory_read_adapter(void *ctx,uint32_t off,uint8_t *p,uint32_t n){
    (void)ctx;fm1_wdt_feed();fm1_irq_off();int rc=FL_FAR(fl_read_ram)(off,p,n);fm1_irq_on();return rc;
}
int core_private_calibration_matches(const unsigned char *expected,unsigned n){
    np_private_match=0;
    if(np_ble_diag_state()||display_dma.busy)return 0;
    np_private_match=np_private_unit_matches(np_factory_read_adapter,0,expected,n,&np_factory_check);
    return (int)np_private_match;
}
/* Fixed read-only status query, never returns calibration payload bytes. */
extern uint32_t np_ble_rf_status(unsigned),np_ble_runtime_status(unsigned);
extern void np_ble_private_probe(void);
static int np_ble_usb(const uint8_t *p,unsigned n){
#ifdef NP_BLUETOOTH_LOADER
    if(np_loader_usb(p,n))return 1;
#endif
    /* Fixed factory region only. Private bytes go to a local paired backup;
     * radio must be off and display DMA idle. No arbitrary address access. */
    if(n==5&&p[0]==0x7d&&p[1]=='N'&&p[2]=='B'&&p[3]==4&&p[4]<128){
        uint8_t chunk=p[4],buffer[64],reply[136]={0xf0,0x7d,'N','B',4,chunk,0};unsigned count=7;
        if(np_ble_diag_state()||!display_ready())reply[6]=1;
        else if(np_factory_read_adapter(0,0xc0000u+(uint32_t)chunk*64,buffer,sizeof buffer))reply[6]=2;
        else for(unsigned i=0;i<sizeof buffer;i++){reply[count++]=buffer[i]&127;reply[count++]=buffer[i]>>7;}
        reply[count++]=0xf7;ota_wire_send(reply,count);return 1;
    }

    if(n!=4||p[0]!=0x7d||p[1]!='N'||p[2]!='B'||p[3]!=3)return 0;
    uint32_t values[]={np_ble_diag_state(),np_ble_diag_error(),
        np_ble_rf_status(0),np_ble_rf_status(1),np_ble_rf_status(2),np_ble_rf_status(3),
        (uint32_t)np_read_result,np_read_count,np_read_off,np_read_size,np_read_sfc,np_read_magic[0],np_read_magic[1],np_private_match,
        np_factory_check.full_match,np_factory_check.first_difference,np_factory_check.differences,np_factory_check.read_failed,
        np_ble_runtime_status(0),np_ble_runtime_status(1),np_ble_runtime_status(2),np_ble_runtime_status(3),np_ble_runtime_status(4)};
    uint8_t response[5+sizeof(values)/sizeof(values[0])*5+1]={0xf0,0x7d,'N','B',3};unsigned k=5;
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)for(unsigned j=0;j<5;j++)response[k++]=(values[i]>>(j*7))&127u;
    response[k++]=0xf7;ota_wire_send(response,k);return 1;
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
    np_ble_private_probe(); /* read-only boot preflight; radio still requires PLAY */
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
