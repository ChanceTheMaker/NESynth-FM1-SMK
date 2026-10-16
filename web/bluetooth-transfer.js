// SPDX-License-Identifier: GPL-3.0-only
import {crc32} from './zip.js';
export function loaderPacket(op,...values){
    const bytes=new Uint8Array(1+4*values.length),view=new DataView(bytes.buffer);
    bytes[0]=op;values.forEach((n,i)=>view.setUint32(1+i*4,n,true));return bytes;
}
export function decodeLoaderStatus(value){
    const v=value instanceof DataView?value:new DataView(value.buffer,value.byteOffset,value.byteLength);
    if(v.byteLength!==20||v.getUint32(0,true)!==0x314c504e)throw Error('This device is not running the NESynth Bluetooth loader.');
    return {state:v.getUint32(4,true),error:v.getUint32(8,true),received:v.getUint32(12,true),capacity:v.getUint32(16,true)};
}
function dataPacket(rom,offset,end){
    const bytes=new Uint8Array(5+end-offset);bytes.set(loaderPacket(2,offset));bytes.set(rom.subarray(offset,end),5);return bytes;
}
// At most eight unacknowledged packets. The cumulative device offset is the
// authority: a lost packet causes a bounded replay of the unfinished window.
// BEGIN/COMMIT retain ATT responses; firmware still verifies the entire CRC.
export async function transferCartridge({rom,session,read,write,burst=false,chunkSize=239,onProgress=()=>{}}){
    if(!session||!Number.isInteger(chunkSize)||chunkSize<1||chunkSize>239)throw Error('Invalid transfer parameters.');
    const initial=await read();
    if(rom.length<16||rom.length>initial.capacity)throw Error('Game exceeds device capacity.');
    await write(Uint8Array.of(4),true);
    await write(loaderPacket(1,session,rom.length,crc32(rom)),true);
    let offset=0,chunk=chunkSize;
    // Confirm usable ATT payload length before issuing unacknowledged data.
    try{const end=Math.min(chunk,rom.length);await write(dataPacket(rom,0,end),true);offset=end;}
    catch(e){
        if(chunk<=15)throw e;
        const s=await read();if(s.state!==1||s.received!==0)throw e;
        chunk=15;const end=Math.min(chunk,rom.length);await write(dataPacket(rom,0,end),true);offset=end;
    }
    onProgress(offset,rom.length);
    while(offset<rom.length){
        const end=Math.min(rom.length,offset+chunk*(burst?8:1));let retries=0;
        for(;;){
            for(let pos=offset;pos<end;){const next=Math.min(end,pos+chunk);await write(dataPacket(rom,pos,next),!burst);pos=next;}
            if(!burst){offset=end;break;}
            const s=await read();
            if(s.state!==1||![0,2].includes(s.error)||s.received<offset||s.received>end)
                throw Error('SMK reported an invalid transfer position. Reconnect and send the game again.');
            offset=s.received;
            if(offset===end){if(s.error)throw Error('SMK reported a transfer error.');break;}
            if(++retries>3)throw Error('Bluetooth transfer stalled. Move the SMK closer and try again.');
        }
        onProgress(offset,rom.length);
    }
    const sent=await read();
    if(sent.state!==1||sent.error||sent.received!==rom.length)throw Error('Transfer incomplete. Reconnect and try again.');
    await write(loaderPacket(3,session),true);
    const verified=await read();
    if(verified.state!==2||verified.error)throw Error('SMK rejected this cartridge.');
    return {bytes:rom.length,burst,chunkSize:chunk};
}
