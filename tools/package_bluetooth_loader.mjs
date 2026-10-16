// SPDX-License-Identifier: GPL-3.0-only
// Local SMK Pro Bluetooth player packaging only. No device access or publication.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {SMKPackage,buildSmkPublicPackage} from '../web/vendor/smk37-package.js';
const folder='build/bluetooth-loader';
const report=JSON.parse(fs.readFileSync(`${folder}/report.json`));
assert.equal(report.kind,'SMK967 Bluetooth loader and player');
assert.equal(report.product,'SMK-37_967');
assert(report.ram_reach_check&&report.cartridge_flash_lo===0x39000&&report.cartridge_flash_hi===0x9d000);
const label=report.private_device_calibration?'PRIVATE-NESynth-SMK-Pro-967-Bluetooth':'NESynth-SMK-Pro-967-Bluetooth';
const app=new Uint8Array(fs.readFileSync(`${folder}/SMK967-BLUETOOTH-RAW.bin`));
assert.equal(createHash('sha256').update(app).digest('hex'),report.sha256);
const stock=new Uint8Array(fs.readFileSync(process.argv[2]||'build/smk37/stock-pro.fwsc'));
// "public" is the existing package validator's calibration-free-app schema;
// this script only writes ignored local outputs, never the website or releases.
const built=await buildSmkPublicPackage(stock,app,{public:true,board:'SMK-37 Pro',product:report.product,
    version:'NESYNTH SMK 967',bytes:app.length,sha256:report.sha256});
const before=new SMKPackage(stock),after=new SMKPackage(built.factory);
assert.deepEqual(after.dec.subarray(after.appOff,after.appOff+app.length),app);
assert.deepEqual(after.flash.subarray(0,0x4000),before.flash.subarray(0,0x4000));
fs.writeFileSync(`${folder}/${label}.fwsc`,built.factory);
fs.writeFileSync(`${folder}/${label}.smkupdate`,built.bytes);
// Clearly synthetic tail, solely for the existing RAM-loader simulation.
const flash=new Uint8Array(0x100000).fill(0xa5);flash.set(before.flash);
const view=new DataView(flash.buffer),trim=0xc1000;
flash.set(new TextEncoder().encode('trimdat\0'),trim+548);
for(let k=0;k<37;k++)for(let j=0;j<5;j++)view.setUint16(trim+168+k*10+j*2,5+j*200,true);
fs.writeFileSync(`${folder}/synthetic-stock.bin`,flash);
fs.writeFileSync(`${folder}/package-report.json`,JSON.stringify({product:built.product,model:'SMK-37 Pro',
    private_device_calibration:!!report.private_device_calibration,app_sha256:report.sha256,fwsc_sha256:createHash('sha256').update(built.factory).digest('hex'),
    update_sha256:built.sha,hardware_validated:false,flashed:false},null,2)+'\n');
console.log('PASS: SMK Pro Bluetooth player package round-trip, exact app bytes and protected boot; local files only.');
