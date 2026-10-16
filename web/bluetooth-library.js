// SPDX-License-Identifier: GPL-3.0-only
import {loaderPacket} from './bluetooth-transfer.js';
import {crc32} from './zip.js';
function view(v){return v instanceof DataView?v:new DataView(v.buffer,v.byteOffset,v.byteLength);}
export function decodeLibrary(value){
    const v=view(value),u=n=>v.getUint32(n,true);
    if(v.byteLength!==40||u(0)!==0x324c504e||u(4)!==2||u(8)>u(28)||u(28)>32||u(12)>u(16)||u(20)>u(24))throw Error('Invalid game library response.');
    return {count:u(8),free:u(12),total:u(16),largest:u(20),gameLimit:u(24),slots:u(28),generation:u(32)};
}
export function decodeGame(value,index){
    const v=view(value);if(v.byteLength!==44)throw Error('Invalid game entry.');
    const bytes=new Uint8Array(v.buffer,v.byteOffset+12,32),end=bytes.indexOf(0);
    if(end<0)throw Error('Invalid game name.');
    return {index,bytes:v.getUint32(4,true),crc:v.getUint32(8,true),name:new TextDecoder().decode(bytes.subarray(0,end))};
}
export async function readLibrary(channel){
    // Older builds do not implement library queries. Do not send destructive commands.
    try{await channel.writeValueWithResponse(Uint8Array.of(6));}catch(e){if(!channel.service.device.gatt.connected)throw e;return null;}
    const library=decodeLibrary(await channel.readValue());library.games=[];
    for(let i=0;i<library.count;i++){
        await channel.writeValueWithResponse(loaderPacket(7,i));library.games.push(decodeGame(await channel.readValue(),i));
    }
    await channel.writeValueWithResponse(Uint8Array.of(0));return library;
}
export function gameNamePacket(name){
    const text=name.replace(/\.[^.]+$/,'').replace(/[^ -~]/g,' ').trim().slice(0,31)||'GAME';
    const p=new Uint8Array(1+text.length);p[0]=5;p.set(new TextEncoder().encode(text),1);return p;
}
export function ensureLibrarySpace(library,rom){
    if(!library)return;
    if(library.games.some(g=>g.bytes===rom.length&&g.crc===crc32(rom)))return;
    if(library.count>=library.slots)throw Error('The game library is full. Delete a game first.');
    if(rom.length>library.largest)throw Error(`This game needs ${Math.ceil(rom.length/1024)} KiB. The largest available slot is ${Math.floor(library.largest/1024)} KiB (${Math.floor(library.free/1024)} KiB total free). Delete a game to make room.`);
}
