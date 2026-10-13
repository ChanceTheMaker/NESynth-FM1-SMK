import fs from 'node:fs/promises';
import assert from 'node:assert/strict';
import {prepareInstall,installFirmware,fm1Identity} from '../web/install-firmware.js?v=20261013';
import {logicalImage,sha256hex} from '../web/vendor/fm1pkg.js';

import {transport} from './fake_transport.mjs';
const bytes=new Uint8Array(await fs.readFile(new URL('../web/firmware/fm1-template.fwsc',import.meta.url)));
const meta=JSON.parse(await fs.readFile(new URL('../web/firmware/template.json',import.meta.url),'utf8'));
const pkg=await prepareInstall(bytes,meta);
for(const start of ['FM-1_930','ota-FM-1_001','Doom-FM-1_007','ota-Doom-FM-1_007']){
 const t=transport({start,restart:meta.product});let confirmations=0;
 assert.equal(await installFirmware({},pkg,()=>{},{Transport:t.Transport,confirmGeneric:async()=>{confirmations++;return true;}}),meta.product);
 assert.equal(confirmations,start==='ota-FM-1_001'?1:0);
 assert.equal(t.calls.includes('stage'),start==='FM-1_930');
 assert.deepEqual(t.Transport.image,logicalImage(bytes));assert.ok(t.links.every(l=>l.closed));
}
for(const [options,error,noWrite] of [
 [{start:null},/No updater/,true],[{start:'SMK-37 Pro_123'},/No updater/,true],
 [{start:'Doom-FM-1_006'},/recovery adapter/,true],
 [{send:false},/disconnected/,true],[{stage:false},/preparation stopped/,true],
 [{loader:false},/has not replied/,true],[{write:false},/Transfer interrupted/,false],
 [{restart:null},/restart was not confirmed/,false],[{restart:'FM-1_999'},/Unexpected firmware/,false]]){
 const t=transport({restart:meta.product,...options});await assert.rejects(installFirmware({},pkg,()=>{},{Transport:t.Transport}),error);
 if(noWrite)assert.ok(!t.calls.includes('write'));assert.ok(t.links.every(l=>l.closed));
}
for(const confirmGeneric of [undefined,async()=>false]){
 const t=transport({start:'ota-FM-1_001'});
 await assert.rejects(installFirmware({},pkg,()=>{},{Transport:t.Transport,confirmGeneric}),/confirmation cancelled/);
 assert.deepEqual(t.calls,['find']);assert.ok(t.links.every(l=>l.closed));
}
await assert.rejects(installFirmware({},Object.freeze({...pkg})),/verified firmware/);
await assert.rejects(prepareInstall(bytes,{...meta,sha256:'bad'}),/integrity/);
await assert.rejects(prepareInstall(bytes,{...meta,product:'SMK_123'}),/integrity/);
const mutable=bytes.slice(),snapshot=await prepareInstall(mutable,{product:meta.product,sha256:await sha256hex(mutable)});
mutable.fill(0);const t=transport({restart:meta.product});await installFirmware({},snapshot,()=>{},{Transport:t.Transport});
assert.deepEqual(t.Transport.image,logicalImage(bytes));
assert.equal(fm1Identity({model:'ota-SMK-37 Pro'}),false);
console.log('PASS: FM-1 / Doom handoffs, active-loader resume, cancellation, interruptions, exact restart, immutable package and cleanup.');
