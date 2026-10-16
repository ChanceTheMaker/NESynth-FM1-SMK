/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint8_t reply[96],written[244];static unsigned reply_n,writes,written_n,read_n=44;
static int reject;
static int np_loader_write(const uint8_t*p,unsigned n){writes++;written_n=n;memcpy(written,p,n);return reject;}
static unsigned np_loader_read(uint16_t off,uint8_t*p,uint16_t n){assert(off==0&&n==44);for(unsigned i=0;i<read_n&&i<n;i++)p[i]=(uint8_t)(i*17);return read_n;}
static int ota_wire_send(const uint8_t*p,unsigned n){assert(n<=sizeof reply);memcpy(reply,p,n);reply_n=n;return 0;}
#include "../experiments/bluetooth/library_usb.h"
int main(void){
 uint8_t request[640]={0x7d,'N','U',11,3,1};
 for(unsigned size=1;size<=244;size++){
  for(unsigned i=0;i<size;i++){uint8_t b=(uint8_t)(i*31+size);request[6+i*2]=b&127;request[7+i*2]=b>>7;}
  assert(np_library_usb_frame(request,6+size*2,1));assert(reply_n==8&&!reply[6]);assert(written_n==size);
  for(unsigned i=0;i<size;i++)assert(written[i]==(uint8_t)(i*31+size));
 }
 unsigned before=writes;assert(np_library_usb_frame(request,494,0)&&reply[6]==2&&writes==before);
 assert(np_library_usb_frame(request,495,1)&&reply[6]==3&&writes==before);
 request[7]=2;assert(np_library_usb_frame(request,8,1)&&reply[6]==3&&writes==before);
 request[7]=0;request[6]=128;assert(np_library_usb_frame(request,8,1)&&reply[6]==3&&writes==before);
 request[6]=0;reject=1;assert(np_library_usb_frame(request,8,1)&&reply[6]==1);reject=0;
 request[5]=0;assert(np_library_usb_frame(request,6,1));assert(reply_n==96&&!reply[6]);
 for(unsigned i=0;i<44;i++)assert((reply[7+i*2]|reply[8+i*2]<<7)==(uint8_t)(i*17));
 assert(reply[4]==11&&reply[5]==3&&reply[95]==0xf7);
 assert(np_library_usb_frame(request,8,1)&&reply[6]==3);
 request[5]=2;assert(np_library_usb_frame(request,6,1)&&reply[6]==3);
 request[3]=255;assert(np_library_usb_frame(request,6,1)&&reply[6]==3&&reply[4]<=127);
 assert(np_library_usb_frame(request,3,1));request[2]='X';assert(!np_library_usb_frame(request,6,1));
 puts("PASS: USB adapter byte round-trip, maximum frame, malformed/oversized rejection, wrong-mode isolation, command errors and sequenced replies");
}
