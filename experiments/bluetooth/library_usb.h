/* SPDX-License-Identifier: GPL-3.0-only */
/* USB MIDI SysEx adapter for the SAME library commands as BLE.
 * F0 7D N U seq-lo seq-hi kind [low7, high1 pairs] F7.
 * kind 0 reads, 1 writes. Replies echo sequence then status and byte pairs.
 * All requests fit the existing 640-byte USB frame; no flash access here. */
#ifndef NP_LIBRARY_USB_H
#define NP_LIBRARY_USB_H
static int np_library_usb_frame(const uint8_t *p,unsigned n,int enabled){
    if(n<3||p[0]!=0x7d||p[1]!='N'||p[2]!='U')return 0;
    if(n<6)return 1;
    uint8_t reply[96]={0xf0,0x7d,'N','U',p[3]&127,p[4]&127,0},raw[244];unsigned count=0,k=7;
    if(!enabled)reply[6]=2;
    else if(n>494||p[3]>127||p[4]>127||p[5]>1||(n-6)%2)reply[6]=3;
    else if(p[5]==0){
        if(n!=6)reply[6]=3;
        else {count=np_loader_read(0,raw,44);if(count>44){reply[6]=3;count=0;}}
    }else{
        unsigned size=(n-6)/2;
        for(unsigned i=0;i<size;i++){
            if(p[6+i*2]>127||p[7+i*2]>1){reply[6]=3;break;}
            raw[i]=(uint8_t)(p[6+i*2]|p[7+i*2]<<7);
        }
        if(!reply[6]&&(!size||np_loader_write(raw,size)))reply[6]=1;
    }
    for(unsigned i=0;i<count;i++){reply[k++]=raw[i]&127;reply[k++]=raw[i]>>7;}
    reply[k++]=0xf7;ota_wire_send(reply,k);return 1;
}
#endif
