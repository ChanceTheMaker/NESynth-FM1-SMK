import fs from 'node:fs/promises';
import assert from 'node:assert/strict';
import {validateStockPackage,logicalImage} from '../web/vendor/fm1pkg.js';
import {restoreOriginal,restoreLatest} from '../web/restore-firmware.js';
import {transport} from './fake_transport.mjs';
const bytes=new Uint8Array(await fs.readFile(process.argv[2]||new URL('../web/firmware/fm1-original.fwsc',import.meta.url)));
assert.equal((await validateStockPackage(bytes)).product,'FM-1_015');
for(const start of ['FM-1_932','ota-FM-1_001','Doom-FM-1_007','ota-Doom-FM-1_007']){
 const t=transport({start,restart:'FM-1_015'});
 assert.equal(await restoreOriginal({},bytes,()=>{},{Transport:t.Transport,confirmGeneric:async()=>true}),'FM-1_015');
 assert.deepEqual(t.Transport.image,logicalImage(bytes));assert.ok(t.links.every(l=>l.closed));
}
const changed=bytes.slice();changed[2000]^=1;
class NoAccess{constructor(){assert.fail('Invalid firmware must never access MIDI');}}
for(const invalid of [changed,bytes.subarray(0,100),new Uint8Array(await fs.readFile(new URL('../web/firmware/fm1-template.fwsc',import.meta.url)))])
 await assert.rejects(restoreOriginal({},invalid,()=>{},{Transport:NoAccess}),/unmodified official/);
const fetcher=async(url,options)=>{assert.equal(url,'firmware/fm1-original.fwsc');assert.equal(options.cache,'no-store');return new Response(bytes);};
const t=transport({restart:'FM-1_015'});
assert.equal(await restoreLatest({},()=>{},{Transport:t.Transport},fetcher),'FM-1_015');
assert.deepEqual(t.Transport.image,logicalImage(bytes));
for(const badFetch of [async()=>new Response('missing',{status:404}),async()=>new Response(changed),async()=>{throw Error('Network unavailable');}])
 await assert.rejects(restoreLatest({},()=>{},{Transport:NoAccess},badFetch));
const wrong=transport({start:'SMK-37 Pro_123'});
await assert.rejects(restoreOriginal({},bytes,()=>{},{Transport:wrong.Transport}),/No updater/);
assert.ok(!wrong.calls.includes('write'));
console.log('PASS: verified official download, exact restore image, all handoffs, wrong model, corrupt/custom/truncated files and failed download rejection. No device accessed.');
