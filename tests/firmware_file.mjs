// Package import only: no MIDI or device access. Fixtures are synthetic.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {Package} from '../web/vendor/fm1pkg.js';
import {SMKPackage} from '../web/vendor/smk37-package.js';
import {prepareFirmwareFile} from '../web/firmware-file.js';
const template=new Uint8Array(await fs.readFile('web/firmware/fm1-template.fwsc'));
const fm=new Package(template);fm.setProduct('FM-1_969');const bytes=fm.serialize(),before=bytes.slice();
const result=await prepareFirmwareFile(bytes,'fm1');assert.equal(result.product,'FM-1_969');assert(result.install);
assert.deepEqual(bytes,before,'Import must not modify the selected file');
await assert.rejects(()=>prepareFirmwareFile(template,'fm1'),/969 or newer/);
await assert.rejects(()=>prepareFirmwareFile(bytes,'smk-pro'),/does not match/);
await assert.rejects(()=>prepareFirmwareFile(bytes,'smk-elite'),/does not match/);
await assert.rejects(()=>prepareFirmwareFile(bytes,'unknown'),/Choose your instrument/);
await assert.rejects(()=>prepareFirmwareFile(new Uint8Array(2*1024*1024+1),'fm1'),/valid NESynth/);
const corrupt=bytes.slice();corrupt[100]^=1;
await assert.rejects(()=>prepareFirmwareFile(corrupt,'fm1'));
for(const [i,file] of process.argv.slice(2).entries()){
 const model=['smk-pro','smk-elite'][i],name=['SMK-37 Pro','SMK-37 Elite'][i];
 const pkg=new SMKPackage(new Uint8Array(await fs.readFile(file)));pkg.setProduct(name+'_969');const candidate=pkg.serialize();
 const prepared=await prepareFirmwareFile(candidate,model);assert.equal(prepared.product,'SMK-37_969');assert(prepared.install);
 await assert.rejects(()=>prepareFirmwareFile(candidate,model==='smk-pro'?'smk-elite':'smk-pro'),/does not match/);
 await assert.rejects(()=>prepareFirmwareFile(candidate,'fm1'));
 const broken=candidate.slice();broken[100]^=1;await assert.rejects(()=>prepareFirmwareFile(broken,model));
}
console.log('PASS: firmware import checks, game-independent preparation, wrong-model, legacy-version and corruption rejection.');
