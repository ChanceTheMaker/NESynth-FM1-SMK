// SPDX-License-Identifier: GPL-3.0-only
// Import repairs only; the emulator and firmware validators remain strict.
// Legacy signature: https://www.nesdev.org/wiki/INES
// Ms. Pac-Man (Tengen) board / PRG / CHR identification:
// https://github.com/mamedev/mame/blob/master/hash/nes.xml (mspacmnt)
const diskDude=[68,105,115,107,68,117,100,101,33];
const msPacmanOverdump='d8fedbf98da736a5dc97a75c32c9c32ba36cf14be55522a49da44dad241a2d1d';
export async function normalizeCartridge(input){
    if(!(input instanceof Uint8Array))throw new TypeError('Expected cartridge bytes.');
    // Exact user-reported legacy dump: preserve mapper, sizes and payload.
    const simpsonsHeader=[78,69,83,26,8,16,17,0,0,0,0,97,115,116,101,114];
    if(input.length===262160&&simpsonsHeader.every((b,i)=>input[i]===b)){
        const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',input.subarray(16))),b=>b.toString(16).padStart(2,'0')).join('');
        if(hash==='346fd8061a0b09c057855536012b146256bfbe386e304ef950ad93603048933c'){
            const bytes=input.slice();bytes.fill(0,11,16);
            return {bytes,notice:'Removed a verified legacy Simpsons header tag. Your original file is unchanged.'};
        }
    }
    if(input.length<16||input[0]!==78||input[1]!==69||input[2]!==83||input[3]!==26||
       !diskDude.every((byte,i)=>input[7+i]===byte))return {bytes:input,notice:''};
    // Only this exact, identified payload may be trimmed or have mirroring changed.
    // First PRG / CHR match MAME SHA1 83057a9b... / d96321e9... respectively.
    if(input.length===65552&&input[4]===2&&input[5]===4&&input[6]===1){
        const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',input.subarray(16))),b=>b.toString(16).padStart(2,'0')).join('');
        if(hash===msPacmanOverdump){
            const bytes=input.slice(0,40976);bytes[5]=1;bytes[6]=0;bytes.fill(0,7,16);
            return {bytes,notice:'Repaired legacy Ms. Pac-Man header and removed excess graphics data. Your original file is unchanged.'};
        }
    }
    // Do not guess an unknown mapper, change payload data, or accept arbitrary junk.
    const bytes=input.slice();bytes.fill(0,7,16);
    return {bytes,notice:'Removed the legacy DiskDude header tag. Your original file is unchanged.'};
}
