/* SPDX-License-Identifier: GPL-3.0-only */
/* Exclusive loader/game modes. The pool is reused only across chip resets.
 * A complete RAM cartridge is CRC-checked again after reset, before any erase.
 * Only the library region is writable; redundant catalogs are committed last. */
#include "rom_transfer.h"
#include "library_store.h"
#define NP_MODE_USB 0x5542504eu
#define NP_MODE_USB_DELETE 0x4542504eu
#define NP_MODE_LOAD 0x4c42504eu
#define NP_MODE_MENU 0x4d42504eu
#define NP_MODE_RECOVERY 0x5242504eu
#define NP_MODE_WRITE 0x5742504eu
#define NP_MODE_PLAY 0x5042504eu
#define NP_MODE_DELETE 0x4442504eu
static uint32_t np_panel_buttons(void){
#if GB_SMK37
    return smk_buttons>>16;
#else
    return fm1_in.buttons>>12;
#endif
}
static uint32_t np_menu_buttons(void){
#if GB_SMK37
    return smk_input_frame();
#else
    uint32_t p=gp_input_keys(key_lo,0),b=np_panel_buttons();
    if(b&1)p|=GP_START;if(b&2)p|=GP_SELECT;return p;
#endif
}
static void np_draw_strip(unsigned y,unsigned rows,const uint16_t *pixels){
#if GB_SMK37
    smk37_panel_window(&panel,0,y,240,rows);lcd_data(pixels,240*rows*2);
#else
    lcd_blit(0,y,240,rows,pixels);lcd_sync(); /* Workspace may be reused immediately. */
#endif
}
#define NP_STAGE ((uint8_t *)0x01c1a000u)
#define NP_UI ((uint16_t *)0x01c5b000u)
#define NP_BLE_WORK ((uint8_t *)0x01c65000u)
typedef struct {uint32_t magic,length,crc,check;char name[32];} np_restart_header;
static volatile np_restart_header np_restart __attribute__((section(".noinit")));
static np_rx np_loader_rx;
static np_store np_library;
static uint32_t np_loader_mode,np_loader_error,np_loader_pending,np_loader_due,np_pending_index,np_pending_crc;
static uint32_t np_installed_length,np_installed_crc,np_query,np_query_index,np_menu_index;
static char np_upload_name[32];
static const uint8_t *np_installed;
static uint32_t np_header_check(uint32_t m,uint32_t n,uint32_t c){return ~(m^n^c);}
static void np_restart_as(uint32_t mode,uint32_t n,uint32_t crc){
    video_stop();np_restart.magic=0;np_restart.length=n;np_restart.crc=crc;
    for(unsigned i=0;i<32;i++)np_restart.name[i]=np_upload_name[i];
    np_restart.check=np_header_check(mode,n,crc)^np_rx_crc32((const uint8_t*)np_upload_name,32);
    __asm__ volatile("csync":::"memory");np_restart.magic=mode;
    __asm__ volatile("csync":::"memory");gb_bootguard.pending=0;fm1_reboot();for(;;);
}
static void np_loader_recover(void){np_restart_as(NP_MODE_RECOVERY,0,0);}
static int np_store_read_flash(void *ctx,uint32_t off,void *p,uint32_t n){
    (void)ctx;fm1_wdt_feed();if(off<NP_LIBRARY_META||off>NP_LIBRARY_END||n>NP_LIBRARY_END-off)return -1;
    return FL_FAR(fl_read_ram)(off,p,n);
}
static int np_store_erase_flash(void *ctx,uint32_t off){
    (void)ctx;uint32_t took;fm1_wdt_feed();
    if(off<NP_LIBRARY_META||off>NP_LIBRARY_END||4096u>NP_LIBRARY_END-off||(off&4095u))return -1;
    return FL_FAR(fl_erase4k_ram)(off,&took);
}
static int np_store_write_flash(void *ctx,uint32_t off,const void *p,uint32_t n){
    (void)ctx;uint32_t took;fm1_wdt_feed();
    if(off<NP_LIBRARY_META||off>NP_LIBRARY_END||n>NP_LIBRARY_END-off)return -1;
    return FL_FAR(fl_prog_ram)(off,p,n,&took);
}
static void np_loader_boot(uint32_t cold){
    uint32_t mode=np_restart.magic,n=np_restart.length,crc=np_restart.crc,check=np_restart.check;
    for(unsigned i=0;i<32;i++)np_upload_name[i]=np_restart.name[i];
    np_restart.magic=0;np_loader_mode=3; /* Library is the normal boot screen. */
    int valid=!cold&&check==(np_header_check(mode,n,crc)^np_rx_crc32((const uint8_t*)np_upload_name,32));
    np_upload_name[31]=0;
    const np_store_io io={0,np_store_read_flash,np_store_erase_flash,np_store_write_flash};
    if(np_store_load(&np_library,&io)){np_loader_error=11;return;}
    if(valid){
        if(mode==NP_MODE_LOAD)np_loader_mode=1;
        if(mode==NP_MODE_USB)np_loader_mode=4;
        if(mode==NP_MODE_RECOVERY)np_loader_mode=2;
        if(mode==NP_MODE_WRITE){
            if(n<16||n>NP_CART_LIMIT||np_validate_rom(NP_STAGE,n)||np_rx_crc32(NP_STAGE,n)!=crc)np_loader_error=10;
            else {int index=np_store_add(&np_library,NP_STAGE,n,crc,np_upload_name);if(index<0)np_loader_error=12;else {n=(uint32_t)index;mode=NP_MODE_PLAY;}}
            if(np_loader_error)np_loader_mode=1;
        }
        if(mode==NP_MODE_DELETE||mode==NP_MODE_USB_DELETE){if(np_store_remove(&np_library,n,crc))np_loader_error=13;np_loader_mode=mode==NP_MODE_USB_DELETE?4:1;}
    }
    SFCENC_CON&=(uint8_t)~2u;SFCENC_UNENC_L=FL_XIP(NP_LIBRARY_META);
    SFCENC_UNENC_H=0x07ffffffu;SFCENC_CON|=2u;fl_inval(NP_LIBRARY_META,0x100000u-NP_LIBRARY_META);
    if(valid&&mode==NP_MODE_PLAY&&n<np_library.catalog.count){
        const np_game *g=&np_library.catalog.games[n];const uint8_t *rom=(const uint8_t*)FL_XIP(g->offset);
        if(g->crc==crc&&!np_validate_rom(rom,g->length)&&np_rx_crc32(rom,g->length)==crc){np_installed=rom;np_installed_length=g->length;np_installed_crc=crc;np_loader_mode=0;}
        else np_loader_error=14;
    }
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
    (void)ctx;(void)n;(void)crc;np_loader_pending=NP_MODE_WRITE;np_loader_due=core_ms()+1500;return 0;
}
static uint32_t np_get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
/* Single GATT value: BEGIN(1, session, length, CRC), DATA(2, offset, bytes),
 * COMMIT(3, session), ABORT(4). All integers little-endian. Status is 20 bytes:
 * NPL1, receiver state, last error, received bytes, capacity. */
int np_loader_write(const uint8_t *p,unsigned n){
    int rc=NP_RX_RANGE;if(!p||!n||np_loader_pending)return 3;np_query=0;
    if(p[0]==0&&n==1)rc=0;
    else if(p[0]==1&&n==13){
        unsigned length=np_get32(p+5),crc=np_get32(p+9);int fits=!!np_store_find(&np_library,length);
        for(unsigned i=0;i<np_library.catalog.count;i++)if(np_library.catalog.games[i].length==length&&np_library.catalog.games[i].crc==crc)fits=1;
        if(fits)rc=np_rx_begin(&np_loader_rx,np_get32(p+1),length,crc);
    }
    else if(p[0]==2&&n>5&&n<=244)rc=np_rx_data(&np_loader_rx,np_loader_rx.session,np_get32(p+1),p+5,n-5,np_rx_crc32(p+5,n-5));
    else if(p[0]==3&&n==5)rc=np_rx_commit(&np_loader_rx,np_get32(p+1));
    else if(p[0]==4&&n==1){np_rx_abort(&np_loader_rx);rc=0;}
    else if(p[0]==5&&n>=2&&n<=32&&np_loader_rx.state!=NP_RX_RECEIVING){
        for(unsigned i=0;i<32;i++)np_upload_name[i]=i+1<n?(char)p[i+1]:0;rc=0;
    }
    else if(p[0]==6&&n==1){np_query=1;rc=0;}
    else if(p[0]==7&&n==5&&np_get32(p+1)<np_library.catalog.count){np_query=2;np_query_index=np_get32(p+1);rc=0;}
    else if((p[0]==8||p[0]==9)&&n==9&&np_loader_rx.state!=NP_RX_RECEIVING){
        unsigned index=np_get32(p+1),crc=np_get32(p+5);
        if(index<np_library.catalog.count&&np_library.catalog.games[index].crc==crc){np_loader_pending=p[0]==8?(np_loader_mode==4?NP_MODE_USB_DELETE:NP_MODE_DELETE):NP_MODE_PLAY;np_pending_index=index;np_pending_crc=crc;np_loader_due=core_ms()+1500;rc=0;}
    }
    else if(p[0]==10&&n==1&&np_loader_rx.state!=NP_RX_RECEIVING){np_loader_pending=NP_MODE_MENU;np_pending_index=np_pending_crc=0;np_loader_due=core_ms()+1500;rc=0;}
    np_loader_error=rc<0?(uint32_t)-rc:0;return rc?0x80:0;
}
static unsigned np_copy_reply(const void *src,unsigned size,uint16_t off,uint8_t *p,uint16_t n){
    if(!p)return size;if(off>=size)return 0;unsigned count=size-off;if(count>n)count=n;
    for(unsigned i=0;i<count;i++)p[i]=((const uint8_t*)src)[off+i];return count;
}
unsigned np_loader_read(uint16_t off,uint8_t *p,uint16_t n){
    if(np_query==1){uint32_t v[]={NP_LIBRARY_MAGIC,2,np_library.catalog.count,np_store_free(&np_library),NP_LIBRARY_BYTES,np_store_largest(&np_library),NP_CART_LIMIT,NP_LIBRARY_MAX,np_library.catalog.sequence,0};return np_copy_reply(v,sizeof v,off,p,n);}
    if(np_query==2)return np_copy_reply(&np_library.catalog.games[np_query_index],sizeof(np_game),off,p,n);
    uint32_t v[]={0x314c504eu,np_loader_rx.state,np_loader_error,np_loader_rx.received,NP_CART_LIMIT};return np_copy_reply(v,sizeof v,off,p,n);
}
void np_loader_connected(void){np_query=0;}
#include "library_usb.h"
static int np_loader_usb(const uint8_t *p,unsigned n){
    if(np_library_usb_frame(p,n,np_loader_mode==4))return 1;
    if(n!=4||p[0]!=0x7d||p[1]!='N'||p[2]!='L')return 0;
    if(p[3]==4&&!np_loader_pending&&np_loader_mode!=4){np_restart_as(NP_MODE_USB,0,0);return 1;}
    if(p[3]==1&&np_loader_mode!=1){np_restart_as(NP_MODE_LOAD,0,0);return 1;}
    if(p[3]==3&&!np_loader_pending){np_restart_as(NP_MODE_MENU,0,0);return 1;}
    if(p[3]!=0&&p[3]!=2)return 0;
    uint32_t v[]={np_loader_mode,np_loader_error,np_installed_length,np_installed_crc,np_loader_rx.received,np_ble_diag_state()};
    if(p[3]==2){v[0]=np_library.catalog.count;v[1]=np_store_free(&np_library);v[2]=NP_LIBRARY_BYTES;v[3]=np_store_largest(&np_library);v[4]=np_menu_index;v[5]=np_loader_mode;}
    uint8_t reply[36]={0xf0,0x7d,'N','L',p[3]};unsigned k=5;
    for(unsigned i=0;i<6;i++)for(unsigned j=0;j<5;j++)reply[k++]=(v[i]>>(7*j))&127;
    reply[k++]=0xf7;ota_wire_send(reply,k);return 1;
}
static void np_loader_controls(void){
    static uint32_t pressed;
    if((np_panel_buttons()&3u)!=3u){pressed=0;return;}
    uint32_t now=core_ms();if(!pressed)pressed=now;
    if(now-pressed>=1200){display_stop();video_stop();audio_stop();np_restart_as(NP_MODE_MENU,0,0);}
}
#include "loader_art.h"
static void np_loader_draw(void){
    uint32_t state=np_ble_diag_state();const char *label=state==4?"CONNECTED":state==3?"SEND A GAME":state==5?"RADIO ERROR":np_loader_mode==4?"USB READY":"STARTING";
    if(np_loader_rx.state==NP_RX_RECEIVING)label="RECEIVING GAME";
    if(np_loader_pending)label=np_loader_pending==NP_MODE_WRITE?"INSTALLING GAME":(np_loader_pending==NP_MODE_DELETE||np_loader_pending==NP_MODE_USB_DELETE)?"DELETING GAME":"STARTING GAME";
    if(np_loader_error){label="UPLOAD ERROR";state=5;}
    unsigned percent=np_loader_rx.length?100u*np_loader_rx.received/np_loader_rx.length:0;
    np_loader_art(NP_UI,176,64,label,percent,state);
    np_draw_strip(176,64,NP_UI);
}
static char *np_dec(char *p,unsigned n){char b[10];unsigned k=0;do{b[k++]=(char)('0'+n%10);n/=10;}while(n);while(k)*p++=b[--k];*p=0;return p;}
static void np_menu_draw(void){
    char space[32],*end=np_dec(space,np_store_free(&np_library)/1024);
    const char *tail="K FREE OF ";while(*tail)*end++=*tail++;end=np_dec(end,NP_LIBRARY_BYTES/1024);*end++='K';*end=0;
    for(unsigned y=0;y<240;y+=60){
        np_loader_art(NP_UI,y,60,"",0,0);np_art a={NP_UI,y,60};np_art_box(&a,0,68,240,172,0x1083);
        np_art_center(&a,70,"GAME LIBRARY",2,0x9cf3);
        unsigned first=np_menu_index/4*4;
        for(unsigned row=0;row<4&&first+row<np_library.catalog.count;row++){
            unsigned i=first+row;const np_game *g=&np_library.catalog.games[i];char name[28];
            for(unsigned j=0;j<27;j++){char c=g->name[j];if(c>='a'&&c<='z')c-=32;name[j]=c;if(!c)break;}name[27]=0;
            if(i==np_menu_index)np_art_box(&a,8,87+row*24,224,22,0x49a8);
            np_art_text(&a,14,93+row*24,name,2,0xef5c);
        }
        if(!np_library.catalog.count)np_art_center(&a,120,"NO GAMES YET",2,0xef5c);
        np_art_center(&a,190,"KEYS SELECT  PLAY GO",2,0xef5c);
        np_art_center(&a,205,space,2,0x45bf);
        np_art_center(&a,224,np_loader_error?"GAME ERROR  REC TO LOAD":"REC BT  USB ON PC",2,np_loader_error?0xe9e8:0x9cf3);
        np_draw_strip(y,60,NP_UI);
    }
}
static void np_menu_run(void){
    uint32_t previous=~0u,last=0;np_menu_draw();
    for(;;){fm1_wdt_feed();recovery_service();uint32_t now=core_ms();if(now-last<25)continue;last=now;
        uint32_t buttons=np_menu_buttons(),pressed=buttons&~previous;previous=buttons;
        if(pressed&GP_SELECT){np_loader_mode=1;return;}
        unsigned count=np_library.catalog.count;
        if(count&&(pressed&(GP_UP|GP_LEFT|GP_DOWN|GP_RIGHT))){np_menu_index=(np_menu_index+((pressed&(GP_UP|GP_LEFT))?count-1:1))%count;np_menu_draw();}
        if(count&&(pressed&(GP_START|GP_A))){const np_game *g=&np_library.catalog.games[np_menu_index];np_restart_as(NP_MODE_PLAY,np_menu_index,g->crc);}
        if(now>=30000&&scan_count>100)smk37_bootguard_healthy(&gb_bootguard);
    }
}
static __attribute__((noreturn)) void np_loader_run(void){
    if(np_loader_mode==3)np_menu_run();
    const np_rx_io io={0,np_stage_begin,np_stage_write,np_stage_read,np_stage_validate,np_stage_publish};
    np_rx_init(&np_loader_rx,&io,NP_CART_LIMIT);
    /* No gameplay buffers/CPU1/audio may run in this mode. */
    for(unsigned y=0;y<240;y+=60){
        np_loader_art(NP_UI,y,60,"STARTING",0,1);
        np_draw_strip(y,60,NP_UI);
    }
    np_loader_draw();if(np_loader_mode!=4)np_ble_diag_start();uint32_t last=0,previous=~0u;
    for(;;){
        fm1_wdt_feed();recovery_service();np_ble_diag_service();uint32_t now=core_ms();
        if(np_loader_pending&&(int32_t)(now-np_loader_due)>=0){display_stop();np_restart_as(np_loader_pending,np_loader_pending==NP_MODE_WRITE?np_loader_rx.length:np_pending_index,np_loader_pending==NP_MODE_WRITE?np_loader_rx.expected_crc:np_pending_crc);}
        uint32_t buttons=np_panel_buttons(),pressed=buttons&~previous;previous=buttons;
        if((pressed&1u)&&!np_loader_pending&&np_loader_rx.state!=NP_RX_RECEIVING)np_restart_as(NP_MODE_MENU,0,0);
        if(now-last>=500){last=now;np_loader_draw();}
        if(now>=30000&&scan_count>100)smk37_bootguard_healthy(&gb_bootguard);
    }
}
