/* SPDX-License-Identifier: GPL-3.0-only */
/* Exclusive loader/game modes. The pool is reused only across chip resets.
 * A complete RAM cartridge is CRC-checked again after reset, before any erase.
 * Only 0x5b000..0x9cfff is writable; metadata is committed last. */
#include "rom_transfer.h"
#define NP_CART_META 0x5b000u
#define NP_CART_DATA 0x5c000u
#define NP_CART_LIMIT 262672u
#define NP_CART_MAGIC 0x3143504eu
#define NP_MODE_LOAD 0x4c42504eu
#define NP_MODE_RECOVERY 0x5242504eu
#define NP_MODE_WRITE 0x5742504eu
#define NP_STAGE ((uint8_t *)0x01c1a000u)
#define NP_UI ((uint16_t *)0x01c5b000u)
#define NP_BLE_WORK ((uint8_t *)0x01c65000u)
typedef struct {uint32_t magic,length,crc,check;} np_cart_header;
static volatile np_cart_header np_restart __attribute__((section(".noinit")));
static np_rx np_loader_rx;
static uint32_t np_loader_mode,np_loader_error,np_loader_pending,np_loader_due;
static uint32_t np_installed_length,np_installed_crc;
static const uint8_t *np_installed;
static uint32_t np_header_check(uint32_t m,uint32_t n,uint32_t c){return ~(m^n^c);}
static void np_restart_as(uint32_t mode,uint32_t n,uint32_t crc){
    video_stop();
    np_restart.magic=0;np_restart.length=n;np_restart.crc=crc;
    np_restart.check=np_header_check(mode,n,crc);
    __asm__ volatile("csync":::"memory");np_restart.magic=mode;
    __asm__ volatile("csync":::"memory");gb_bootguard.pending=0;fm1_reboot();for(;;);
}
static void np_loader_recover(void){np_restart_as(NP_MODE_RECOVERY,0,0);}
static int np_program_cartridge(uint32_t n,uint32_t crc){
    uint32_t took;uint8_t check[256];
    /* Already radio-off following chip reset. Interrupts remain disabled. */
    if(FL_FAR(fl_erase4k_ram)(NP_CART_META,&took))return 1;
    for(uint32_t off=0;off<n;off+=4096){
        fm1_wdt_feed();if(FL_FAR(fl_erase4k_ram)(NP_CART_DATA+off,&took))return 2;
    }
    for(uint32_t off=0;off<n;){
        uint32_t count=n-off;if(count>256)count=256;fm1_wdt_feed();
        if(FL_FAR(fl_prog_ram)(NP_CART_DATA+off,NP_STAGE+off,count,&took))return 3;
        if(FL_FAR(fl_read_ram)(NP_CART_DATA+off,check,count))return 4;
        for(unsigned i=0;i<count;i++)if(check[i]!=NP_STAGE[off+i])return 5;
        off+=count;
    }
    np_cart_header header={NP_CART_MAGIC,n,crc,np_header_check(NP_CART_MAGIC,n,crc)};
    if(FL_FAR(fl_prog_ram)(NP_CART_META,(const uint8_t *)&header,sizeof header,&took))return 6;
    if(FL_FAR(fl_read_ram)(NP_CART_META,check,sizeof header))return 7;
    for(unsigned i=0;i<sizeof header;i++)if(check[i]!=((const uint8_t *)&header)[i])return 8;
    return 0;
}
static void np_loader_boot(uint32_t cold){
    uint32_t mode=np_restart.magic,n=np_restart.length,crc=np_restart.crc,check=np_restart.check;
    np_restart.magic=0;
    if(!cold&&check==np_header_check(mode,n,crc)){
        np_loader_mode=mode==NP_MODE_LOAD?1:mode==NP_MODE_RECOVERY?2:0;
        if(mode==NP_MODE_WRITE){
            if(n<16||n>NP_CART_LIMIT||np_validate_rom(NP_STAGE,n)||np_rx_crc32(NP_STAGE,n)!=crc)
                np_loader_error=10; /* Reset did not preserve staging: old game untouched. */
            else np_loader_error=np_program_cartridge(n,crc);
            if(np_loader_error)np_loader_mode=1;
        }
    }
    /* The cartridge is plaintext; code and calibration remain encrypted as before. */
    SFCENC_CON&=(uint8_t)~2u;SFCENC_UNENC_L=FL_XIP(NP_CART_META);
    SFCENC_UNENC_H=0x07ffffffu;SFCENC_CON|=2u;
    fl_inval(NP_CART_META,0x100000u-NP_CART_META);
    const volatile np_cart_header *h=(const volatile np_cart_header *)FL_XIP(NP_CART_META);
    n=h->length;crc=h->crc;
    if(h->magic==NP_CART_MAGIC&&n>=16&&n<=NP_CART_LIMIT&&h->check==np_header_check(NP_CART_MAGIC,n,crc)){
        const uint8_t *rom=(const uint8_t *)FL_XIP(NP_CART_DATA);fm1_wdt_feed();
        if(!np_validate_rom(rom,n)&&np_rx_crc32(rom,n)==crc){np_installed=rom;np_installed_length=n;np_installed_crc=crc;}
    }
    if(!np_installed&&!np_loader_mode)np_loader_mode=1;
}
static int np_stage_begin(void *ctx,uint32_t n){(void)ctx;(void)n;np_loader_pending=0;np_loader_error=0;return 0;}
static int np_stage_write(void *ctx,uint32_t off,const uint8_t *p,uint32_t n){
    (void)ctx;if(off>NP_CART_LIMIT||n>NP_CART_LIMIT-off)return -1;
    for(unsigned i=0;i<n;i++)NP_STAGE[off+i]=p[i];return 0;
}
static int np_stage_read(void *ctx,uint32_t off,uint8_t *p,uint32_t n){
    (void)ctx;if(off>NP_CART_LIMIT||n>NP_CART_LIMIT-off)return -1;
    for(unsigned i=0;i<n;i++)p[i]=NP_STAGE[off+i];return 0;
}
static int np_stage_validate(void *ctx,uint32_t n){(void)ctx;return np_validate_rom(NP_STAGE,n);}
static int np_stage_publish(void *ctx,uint32_t n,uint32_t crc){
    (void)ctx;(void)n;(void)crc;np_loader_pending=1;np_loader_due=core_ms()+1500;return 0;
}
static uint32_t np_get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
/* Single GATT value: BEGIN(1, session, length, CRC), DATA(2, offset, bytes),
 * COMMIT(3, session), ABORT(4). All integers little-endian. Status is 20 bytes:
 * NPL1, receiver state, last error, received bytes, capacity. */
int np_loader_write(const uint8_t *p,unsigned n){
    int rc=NP_RX_RANGE;if(!p||!n||np_loader_pending)return 3;
    if(p[0]==1&&n==13)rc=np_rx_begin(&np_loader_rx,np_get32(p+1),np_get32(p+5),np_get32(p+9));
    else if(p[0]==2&&n>5&&n<=244)rc=np_rx_data(&np_loader_rx,np_loader_rx.session,np_get32(p+1),p+5,n-5,np_rx_crc32(p+5,n-5));
    else if(p[0]==3&&n==5)rc=np_rx_commit(&np_loader_rx,np_get32(p+1));
    else if(p[0]==4&&n==1){np_rx_abort(&np_loader_rx);rc=0;}
    np_loader_error=rc<0?(uint32_t)-rc:0;return rc?0x80:0;
}
unsigned np_loader_read(uint16_t off,uint8_t *p,uint16_t n){
    uint32_t v[]={0x314c504eu,np_loader_rx.state,np_loader_error,np_loader_rx.received,NP_CART_LIMIT};
    if(!p)return sizeof v;if(off>=sizeof v)return 0;
    unsigned count=sizeof v-off;if(count>n)count=n;
    for(unsigned i=0;i<count;i++)p[i]=((const uint8_t *)v)[off+i];return count;
}
static int np_loader_usb(const uint8_t *p,unsigned n){
    if(n!=4||p[0]!=0x7d||p[1]!='N'||p[2]!='L')return 0;
    if(p[3]==1&&!np_loader_mode){np_restart_as(NP_MODE_LOAD,0,0);return 1;}
    if(p[3]!=0)return 0;
    uint32_t v[]={np_loader_mode,np_loader_error,np_installed_length,np_installed_crc,np_loader_rx.received,np_ble_diag_state()};
    uint8_t reply[36]={0xf0,0x7d,'N','L',0};unsigned k=5;
    for(unsigned i=0;i<6;i++)for(unsigned j=0;j<5;j++)reply[k++]=(v[i]>>(7*j))&127;
    reply[k++]=0xf7;ota_wire_send(reply,k);return 1;
}
static void np_loader_controls(void){
    static uint32_t pressed;
    if((smk_buttons&(3u<<16))!=(3u<<16)){pressed=0;return;}
    uint32_t now=core_ms();if(!pressed)pressed=now;
    if(now-pressed>=1200){display_stop();video_stop();audio_stop();np_restart_as(NP_MODE_LOAD,0,0);}
}
static void np_loader_line(unsigned y,const char *s){
    for(unsigned x=12;*s&&x+9<240;s++,x+=12){
        unsigned c=(unsigned char)*s,k=c>='0'&&c<='9'?c-'0':c>='A'&&c<='Z'?c-'A'+10:99;
        if(k>=36)continue;
        for(unsigned r=0;r<5;r++)for(unsigned col=0;col<3;col++)if(np_font[k][r]&(4u>>col))
            for(unsigned dy=0;dy<3;dy++)for(unsigned dx=0;dx<3;dx++)NP_UI[(y+r*3+dy)*240+x+col*3+dx]=0xffff;
    }
}
static void np_loader_draw(void){
    for(unsigned i=0;i<240*64;i++)NP_UI[i]=0x0821;
    np_loader_line(4,"NESYNTH BLUETOOTH");
    uint32_t state=np_ble_diag_state();const char *label=state==4?"CONNECTED":state==3?"SEND A GAME":state==5?"RADIO ERROR":"STARTING";
    if(np_loader_rx.state==NP_RX_RECEIVING)label="RECEIVING GAME";
    if(np_loader_pending)label="INSTALLING GAME";
    if(np_loader_error)label="UPLOAD ERROR";
    np_loader_line(25,label);
    unsigned percent=np_loader_rx.length?100u*np_loader_rx.received/np_loader_rx.length:0;
    for(unsigned y=49;y<57;y++)for(unsigned x=12;x<12+percent*2;x++)NP_UI[y*240+x]=0x07e0;
    smk37_panel_window(&panel,0,0,240,64);lcd_data(NP_UI,240*64*2);
}
static __attribute__((noreturn)) void np_loader_run(void){
    const np_rx_io io={0,np_stage_begin,np_stage_write,np_stage_read,np_stage_validate,np_stage_publish};
    np_rx_init(&np_loader_rx,&io,NP_CART_LIMIT);
    /* No gameplay buffers/CPU1/audio may run in this mode. */
    for(unsigned i=0;i<240*64;i++)NP_UI[i]=0x0821;
    for(unsigned y=0;y<240;y+=60){smk37_panel_window(&panel,0,y,240,60);lcd_data(NP_UI,240*60*2);}
    np_loader_draw();np_ble_diag_start();uint32_t last=0;
    for(;;){
        fm1_wdt_feed();recovery_service();np_ble_diag_service();uint32_t now=core_ms();
        if(np_loader_pending&&(int32_t)(now-np_loader_due)>=0){display_stop();np_restart_as(NP_MODE_WRITE,np_loader_rx.length,np_loader_rx.expected_crc);}
        if(now-last>=500){last=now;np_loader_draw();}
        if(now>=30000&&scan_count>100)smk37_bootguard_healthy(&gb_bootguard);
    }
}
