// SPDX-License-Identifier: GPL-3.0-only
export const HEADER=1024,LO=0x4000,HI=0x9d000;
export function crc32(data){let c=0xffffffff;for(const b of data){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((0-(c&1))&0xedb88320);}return (c^0xffffffff)>>>0;}
export function makeSmkPacket(flash,product,version){
 if(!(flash instanceof Uint8Array)||flash.length!==HI||crc32(flash.subarray(0,LO))!==0x3a04ad23||!/^SMK-37_\d{3}$/.test(product)||!version||version.length>31)throw Error('Invalid SMK flash profile.');
 const bytes=new Uint8Array(HEADER+HI-LO),h=new DataView(bytes.buffer);
 bytes.set(new TextEncoder().encode('SMK37-SALT-USB1'));
 [2,LO,HI,HI-LO,0x3a04ad23,0,crc32(flash.subarray(LO))].forEach((v,i)=>h.setUint32(16+i*4,v,true));
 bytes.set(new TextEncoder().encode(product),48);bytes.set(new TextEncoder().encode(version),80);bytes.set(flash.subarray(LO),HEADER);
 for(let i=0;i<(HI-LO)/4096;i++)h.setUint32(128+i*4,crc32(bytes.subarray(HEADER+i*4096,HEADER+(i+1)*4096)),true);
 h.setUint32(44,crc32(bytes.subarray(0,HEADER)),true);return bytes;
}
