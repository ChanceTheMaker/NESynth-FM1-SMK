import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {normalizeCartridge} from '../web/cartridge.js';
import {openCartridge} from '../web/zip.js';
import {prepareFirmware,validateHardwareRom} from '../web/rom-firmware.js';
import {Package} from '../web/vendor/fm1pkg.js';
const sha=b=>createHash('sha256').update(b).digest('hex');
const diagnostic=new Uint8Array(fs.readFileSync('web/diagnostic.nes'));
const tag=new TextEncoder().encode('DiskDude!');
const dirty=diagnostic.slice();dirty.set(tag,7);const original=dirty.slice();
assert.throws(()=>validateHardwareRom(dirty),/iNES/);
const repaired=await normalizeCartridge(dirty);
assert.deepEqual(repaired.bytes,diagnostic);assert.match(repaired.notice,/DiskDude/);
assert.deepEqual(dirty,original,'Do not mutate the caller\'s cartridge');
assert.equal((await normalizeCartridge(diagnostic)).bytes,diagnostic);
assert.equal((await normalizeCartridge(repaired.bytes)).notice,'');
for(const bad of [diagnostic.slice(0,10),(()=>{const b=dirty.slice();b[15]=0;return b;})(),(()=>{const b=diagnostic.slice();b[7]=8;return b;})(),(()=>{const b=diagnostic.slice();b[7]=1;return b;})()]){
    assert.equal((await normalizeCartridge(bad)).bytes,bad,'Unknown formats must stay unchanged');
    assert.throws(()=>validateHardwareRom(bad));
}
// An unknown NROM-sized-looking overdump must not be silently truncated.
const unknown=new Uint8Array(65552);unknown.set([78,69,83,26,2,4,1]);unknown.set(tag,7);
const untouched=(await normalizeCartridge(unknown)).bytes;
assert.equal(untouched.length,65552);assert.equal(untouched[5],4);assert.equal(untouched[6],1);
assert.throws(()=>validateHardwareRom(untouched),/NROM/);
const template=new Uint8Array(fs.readFileSync('web/firmware/fm1-template.fwsc'));
const metadata=JSON.parse(fs.readFileSync('web/firmware/template.json'));
async function checkFirmware(input,expected){
    const built=await prepareFirmware(input,template,metadata),pkg=new Package(built.bytes);
    const start=pkg.appOff+metadata.cartridge_offset;
    assert.deepEqual(pkg.dec.subarray(start,start+expected.length),expected);
    assert.equal(built.rom_sha256,sha(expected));
}
await checkFirmware(dirty,diagnostic);
console.log('PASS: exact DiskDude repair, immutable input, idempotence, strict unknown/oversized/NES 2.0/console rejection, repaired firmware bytes.');
if(process.argv[2]){
    const filename=process.argv[2],before=fs.readFileSync(filename);
    const entries=await openCartridge(new File([before],path.basename(filename)));assert.equal(entries.length,1);
    const raw=await entries[0].read(),result=await normalizeCartridge(raw);
    assert.equal(raw.length,65552);assert.equal(result.bytes.length,40976);
    assert.equal(sha(result.bytes),'8d2fef447de99528fb18671c8f74b2fd45bf3c4bb4bfc5b16627b0128fdab9ce');
    assert.match(result.notice,/Ms. Pac-Man/);validateHardwareRom(result.bytes);
    await checkFirmware(raw,result.bytes);
    const g=(await WebAssembly.instantiate(fs.readFileSync('web/nes.wasm'),{})).instance.exports;
    new Uint8Array(g.memory.buffer,g.gp_upload(),result.bytes.length).set(result.bytes);
    assert.equal(g.gp_load_upload(result.bytes.length),0);
    let sound=false;
    for(let frame=1;frame<=3600;frame++){
        g.gp_buttons([400,900,1600,2200].some(n=>frame>=n&&frame<n+10)?8:frame>2500?32:0);
        assert.equal(g.gp_frame(),1);assert.equal(g.gp_error(),0);
        const count=g.gp_audio_queued(),ptr=g.gp_render_audio(count);
        sound ||= new Int16Array(g.memory.buffer,ptr,count*2).some(v=>v!==0);
    }
    assert(sound,'Game must boot far enough to produce audio');
    assert(new Set(new Uint8Array(g.memory.buffer,g.gp_pixels(),256*240)).size>5,'Game must render beyond the blank startup screen');
    const pythonHash=execFileSync('python',['-c',"import sys,hashlib;sys.path.insert(0,'tools');from cartridge import read_cartridge;print(hashlib.sha256(read_cartridge(sys.argv[1])).hexdigest())",filename],{encoding:'utf8'}).trim();
    assert.equal(pythonHash,sha(result.bytes));
    const modified=raw.slice();modified[100]^=1;
    assert.equal((await normalizeCartridge(modified)).bytes.length,65552,'Only the checksum-verified dump may be trimmed');
    assert.deepEqual(fs.readFileSync(filename),before,'The original ZIP must remain unchanged');
    console.log('PASS: private Ms. Pac-Man ZIP, 3600 frames with picture/audio, canonical PRG/CHR, Python/browser parity, firmware packing, changed-payload rejection, original archive unchanged.');
}
