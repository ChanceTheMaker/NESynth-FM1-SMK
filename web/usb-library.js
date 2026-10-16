// SPDX-License-Identifier: GPL-3.0-only
// Cartridge commands over USB MIDI. Never sends a firmware-upgrade command.
import {Updater} from './vendor/fm1ota.js';
const PREFIX=[0xf0,0x7d,78,85];
export function usbRequest(sequence,kind,bytes=new Uint8Array()){
    if(!Number.isInteger(sequence)||sequence<0||sequence>16383||![0,1].includes(kind)||bytes.length>244||(!kind&&bytes.length))throw Error('Invalid USB library request.');
    const out=[...PREFIX,sequence&127,sequence>>>7,kind];for(const b of bytes)out.push(b&127,b>>>7);out.push(0xf7);return out;
}
export function usbReply(packet,sequence){
    if(!packet||packet.length<8||!PREFIX.every((b,i)=>packet[i]===b)||packet[4]!== (sequence&127)||packet[5]!==sequence>>>7)return null;
    if(packet.length>96||packet.at(-1)!==0xf7||(packet.length-8)%2||packet.slice(1,-1).some(b=>b>127))throw Error('Malformed USB library reply.');
    if(packet[6])throw Error(packet[6]===2?'Device is not in USB loading mode. Reconnect using USB.':packet[6]===1?'Device rejected the game command. Check available space and reconnect.':'Malformed USB game command.');
    const out=new Uint8Array((packet.length-8)/2);
    for(let i=0;i<out.length;i++){if(packet[8+i*2]>1)throw Error('Malformed USB library byte.');out[i]=packet[7+i*2]|packet[8+i*2]<<7;}return out;
}
export function createUsbChannel(link){
    let sequence=0,closed=false,pending=false;const listeners=[];
    const device={transport:'USB',addEventListener:(event,fn)=>{if(event==='gattserverdisconnected')listeners.push(fn);},gatt:{get connected(){return !closed&&!link.lost;},disconnect(){if(closed)return;closed=true;void link.close();listeners.forEach(fn=>fn());}}};
    async function exchange(kind,bytes){
        if(pending)throw Error('A USB game command is already in progress.');
        if(!device.gatt.connected)throw Error('USB disconnected.');pending=true;
        try{
            const id=sequence=(sequence+1)&16383;link.drain();if(!link.send(usbRequest(id,kind,bytes)))throw Error('USB disconnected.');
            const end=Date.now()+3000;
            while(Date.now()<end){if(link.lost)throw Error('USB disconnected.');const p=await link.read(Math.min(200,end-Date.now())),data=usbReply(p,id);if(data!==null)return data;}
            throw Error('USB game command timed out. Reconnect and try again.');
        }finally{pending=false;}
    }
    return {device,characteristic:{service:{device},properties:{writeWithoutResponse:false},writeValueWithResponse:bytes=>exchange(1,bytes),readValue:async()=>{const b=await exchange(0);return new DataView(b.buffer,b.byteOffset,b.byteLength);}}};
}
async function mode(link){
    const prefix=[0xf0,0x7d,78,76,0];link.drain();if(!link.send([...prefix,0xf7]))throw Error('USB disconnected.');const end=Date.now()+2500;
    while(Date.now()<end){const p=await link.read(200);if(!p||!prefix.every((b,i)=>p[i]===b))continue;
        if(p.length!==36||p[35]!==0xf7||p.slice(1,-1).some(b=>b>127)||p[9]>15)throw Error('Malformed device mode reply.');
        return p[5]|p[6]<<7|p[7]<<14|p[8]<<21|p[9]<<28;
    }throw Error('Device did not answer the USB library query.');
}
export async function connectUsbLibrary(status=()=>{}){
    if(!navigator.requestMIDIAccess)throw Error('Use Chrome or Edge with a USB data cable for USB loading.');
    const access=await navigator.requestMIDIAccess({sysex:true});if(access.sysexEnabled===false)throw Error('Allow MIDI/SysEx access to upload games over USB.');
    const updater=new Updater(access,()=>{},/fm-?1|smk|nesynth|ota|composite|sinco|usb-midi/i);let dev;
    try{
        dev=await updater.findAny(id=>['FM-1','SMK-37'].includes(id.model));if(!dev)throw Error('FM-1 or SMK not found over USB. Check the data cable and close other MIDI installers.');
        const model=dev.id.model,minimum=model==='FM-1'?969:968;
        if(dev.id.version<minimum)throw Error(`USB game loading requires NESynth ${model} build ${minimum} or newer.`);
        if(await mode(dev.link)!==4){
            status(`Opening USB game loading on your ${model}...`);
            if(!dev.link.send([0xf0,0x7d,78,76,4,0xf7]))throw Error('USB disconnected.');
            await new Promise(r=>setTimeout(r,400));await dev.link.close();dev=null;
            dev=await updater.waitForAny(id=>id.model===model&&id.version>=minimum,25000);
            if(!dev||await mode(dev.link)!==4)throw Error('Device did not reopen in USB loading mode. Reconnect and try again.');
        }
        const result=createUsbChannel(dev.link);
        const changed=()=>{if(dev.link.lost){result.device.gatt.disconnect();access.removeEventListener?.('statechange',changed);}};
        access.addEventListener?.('statechange',changed);result.device.addEventListener('gattserverdisconnected',()=>access.removeEventListener?.('statechange',changed));return result;
    }catch(e){if(dev)await dev.link.close();throw e;}
}
