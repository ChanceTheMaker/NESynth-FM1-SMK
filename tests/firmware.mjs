import fs from 'node:fs/promises';
import assert from 'node:assert/strict';
import {prepareFirmware,validateHardwareRom,protectedAppHash} from '../web/rom-firmware.js';
import {Package,sha256hex} from '../web/vendor/fm1pkg.js';
import {openCartridge} from '../web/zip.js';
import path from 'node:path';
const read=async path=>new Uint8Array(await fs.readFile(new URL(path,import.meta.url)));
const template=await read('../web/firmware/fm1-template.fwsc');
const metadata=JSON.parse(await fs.readFile(new URL('../web/firmware/template.json',import.meta.url),'utf8'));
const diagnostic=await read('../web/diagnostic.nes');
const mmc2=new Uint8Array(262672);mmc2.set([78,69,83,26,8,16,0x94]);mmc2.fill(0x37,16);
const mmc1=new Uint8Array(393744);mmc1.set([78,69,83,26,16,16,0x14]);mmc1.fill(0x41,16);
const {cartridge_offset:offset,cartridge_capacity:capacity}=metadata;
const uxrom=new Uint8Array(16+256*1024);uxrom.set([78,69,83,26,16,0,0x20]);
const cnrom=new Uint8Array(16+64*1024);cnrom.set([78,69,83,26,2,4,0x30]);
const mmc3=new Uint8Array(524816);mmc3.set([78,69,83,26,16,32,0x44]);mmc3.fill(0x43,16);
const colorDreams=new Uint8Array(16+256*1024);colorDreams.set([78,69,83,26,8,16,0xb0]);
const cartridges=[diagnostic,mmc1,mmc2,uxrom,cnrom,mmc3,colorDreams];
if(process.argv[2]){
 const input=process.argv[2],entries=await openCartridge(new File([await fs.readFile(input)],path.basename(input)));
 assert.equal(entries.length,1);cartridges.push(await entries[0].read());
}
for(const rom of cartridges){
 const built=await prepareFirmware(rom,template,metadata),pkg=new Package(built.bytes);
 const app=pkg.dec.subarray(pkg.appOff,pkg.appOff+pkg.app.size);
 assert.deepEqual(app.subarray(offset,offset+rom.length),rom);
 assert(app.subarray(offset+rom.length,offset+capacity).every(b=>b===255));
 assert.equal(await protectedAppHash(app,offset,capacity),metadata.protected_app_sha256);
 assert.equal(built.sha256,await sha256hex(built.bytes));assert.equal(built.usb_updater,true);
}
for(const [at,value,error] of [[0,0,/iNES/],[7,8,/iNES/],[6,0x50,/mapper/],[8,2,/RAM/],[9,1,/PAL/],[10,1,/iNES/],[4,3,/size/]]){
 const bad=diagnostic.slice();bad[at]=value;assert.throws(()=>validateHardwareRom(bad),error);
}
assert.throws(()=>validateHardwareRom(diagnostic.subarray(0,15)),/shorter/);
assert.throws(()=>validateHardwareRom(diagnostic.subarray(0,100)),/size/);
const invalidMMC2=mmc2.slice();invalidMMC2[6]|=8;assert.throws(()=>validateHardwareRom(invalidMMC2),/mirroring/);
for(const changes of [{sha256:'bad'},{cartridge_offset:-1},{cartridge_capacity:524288},{usb_updater:false},{protected_app_sha256:'bad'},{loader_sha256:'bad'}])
 await assert.rejects(prepareFirmware(diagnostic,template,{...metadata,...changes}));
const damaged=template.slice();damaged[2000]^=1;await assert.rejects(prepareFirmware(diagnostic,damaged,metadata),/integrity/);
const oversized=new Uint8Array(16+512*1024+256*1024);oversized.set([78,69,83,26,32,32,0x40]);
validateHardwareRom(oversized);await assert.rejects(prepareFirmware(oversized,template,metadata),/too large/);
await assert.rejects(prepareFirmware(mmc3,template,{...metadata,mappers:[0,1,9]}),/does not support/);
console.log('PASS: all seven mappers, maximum 512 KiB cartridge packing, exact bytes, updater retention, oversized device ROM and stale template rejection.');
