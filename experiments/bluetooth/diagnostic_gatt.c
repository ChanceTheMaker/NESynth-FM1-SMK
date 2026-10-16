/* SPDX-License-Identifier: GPL-3.0-only */
/* JieLi ATT table layout follows Felucca's experimental ble_midi_service.c.
 * This is a distinct diagnostic service: an 8-byte RAM echo, no cartridge
 * writes, pairing, bonding, notifications or asynchronous send callbacks. */
#include "diagnostic_gatt.h"
#define UUID(last) last,0xe7,0xab,0x7e,0xf4,0xd6,0x4c,0xbc,0xc4,0x4b,0x8b,0xcf,0xf1,0x4c,0x6c,0xf8
const uint8_t np_diag_db[]={
    10,0,2,0,1,0,0,0x28,0,0x18,
    13,0,2,0,2,0,3,0x28,2,3,0,0,0x2a,
    8,0,2,1,3,0,0,0x2a,
    24,0,2,0,4,0,0,0x28,UUID(0xe0),
    27,0,2,0,5,0,3,0x28,0x0a,6,0,UUID(0xe1),
    22,0,0x0a,3,6,0,UUID(0xe1),0,0
};
const uint8_t np_diag_adv[]={2,1,6,17,7,UUID(0xe0)};
const uint8_t np_diag_rsp[]={17,9,'N','E','S','y','n','t','h',' ','S','M','K',' ','T','e','s','t'};
const unsigned np_diag_adv_size=sizeof np_diag_adv,np_diag_rsp_size=sizeof np_diag_rsp;
void np_diag_connect(np_diag_gatt *s,uint16_t h){*s=(np_diag_gatt){0};s->handle=h;s->connected=1;}
void np_diag_disconnect(np_diag_gatt *s,uint16_t h){if(s->connected&&s->handle==h)*s=(np_diag_gatt){0};}
/* AC79 HCI/L2CAP callbacks report event[1]+1 for fixed lifecycle events.
 * As in the previously tested Felucca adapter, permit that convention only
 * at this controller boundary; never read beyond the actual buffer size. */
int np_diag_event(np_diag_gatt *s,uint8_t type,const uint8_t *p,uint16_t n,int controller){
    if(type!=4||!p||n<2)return 0;
    unsigned declared=(unsigned)p[1]+2;
    int connection=p[0]==0x3e&&n>=6&&((p[2]==1&&declared==21)||(p[2]==10&&declared==33));
    int disconnect=p[0]==5&&declared==6&&n>=5;
    if(n<declared&&!(controller&&n+1u==declared&&(connection||disconnect)))return 0;
    if(connection){
        if(p[3])return s->connected?0:3;
        if(s->connected)return 0; /* duplicate HCI/L2CAP delivery */
        np_diag_connect(s,(uint16_t)(p[4]|p[5]<<8));return 1;
    }
    if(disconnect&&!p[2]&&s->connected&&s->handle==(uint16_t)(p[3]|p[4]<<8)){
        np_diag_disconnect(s,s->handle);return 2;
    }
    return 0;
}
uint16_t np_diag_read(np_diag_gatt *s,uint16_t h,uint16_t attr,uint16_t off,uint8_t *buf,uint16_t n){
    static const uint8_t name[]=NP_BLE_NAME;
    uint8_t value[16]={'N','P','B','1'};const uint8_t *p;unsigned size;
    if(!s->connected||h!=s->handle)return 0;
    if(attr==3){p=name;size=sizeof name-1;}
    else if(attr==6){
        for(unsigned i=0;i<4;i++)value[4+i]=(uint8_t)(s->writes>>(8*i));
        for(unsigned i=0;i<8;i++)value[8+i]=s->echo[i];
        p=value;size=sizeof value;
    }else return 0;
    if(!buf)return (uint16_t)size;
    if(off>=size)return 0;
    size-=off;if(n<size)size=n;
    for(unsigned i=0;i<size;i++)buf[i]=p[off+i];
    return (uint16_t)size;
}
int np_diag_write(np_diag_gatt *s,uint16_t h,uint16_t attr,uint16_t mode,uint16_t off,const uint8_t *p,uint16_t n){
    if(!s->connected||h!=s->handle||attr!=6)return 3; /* write not permitted */
    if(mode)return 6; /* no prepared/queued transactions */
    if(off)return 7;
    if(!p||n!=8)return 13;
    for(unsigned i=0;i<8;i++)s->echo[i]=p[i];
    s->writes++;return 0;
}
