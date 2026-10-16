// Exact model packages and simulated MIDI only. Never accesses a device.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {prepareSmkFirmware,prepareSmkRestore,getSmkStock} from '../web/smk-firmware.js';
import {installSmkFirmware,smkIdentity} from '../web/smk-install.js';
import {SMKPackage} from '../web/vendor/smk37-package.js';
const template=new Uint8Array(fs.readFileSync('web/firmware/smk37-template.bin')),meta=JSON.parse(fs.readFileSync('web/firmware/smk37-template.json'));
const rom=new Uint8Array(fs.readFileSync('web/diagnostic.nes'));
const fixtures=process.argv.slice(2);assert.equal(fixtures.length,2,'Pass pinned Pro016 and Elite016 fixture paths.');
fs.mkdirSync('build/smk37',{recursive:true});
for(const [i,model] of ['SMK-37 Pro','SMK-37 Elite'].entries()){
 const stock=new Uint8Array(fs.readFileSync(fixtures[i])),before=new SMKPackage(stock);
 const built=await prepareSmkFirmware(rom,template,meta,stock,model),after=new SMKPackage(built.bytes);
 assert.equal(built.product,meta.product);assert.equal(after.product,model+'_'+meta.product.split('_')[1]);
 assert.deepEqual(after.flash.subarray(0,0x4000),before.flash.subarray(0,0x4000));
 assert.deepEqual(after.dec.subarray(after.appOff+meta.cartridge_offset,after.appOff+meta.cartridge_offset+rom.length),rom);
 assert(after.dec.subarray(after.appOff+meta.cartridge_offset+rom.length,after.appOff+meta.cartridge_offset+meta.cartridge_capacity).every(x=>x===255));
 const restore=await prepareSmkRestore(stock,model);assert.equal(restore.expectedProduct,model+'_016');
 const fakeStock=stock.slice();fakeStock[100]^=1;
 await assert.rejects(prepareSmkRestore(fakeStock,model),/does not match/);
 await assert.rejects(prepareSmkFirmware(rom,template,meta,stock,i?'SMK-37 Pro':'SMK-37 Elite'),/does not match/);
 await assert.rejects(prepareSmkFirmware(rom,template,{...meta,protected_app_sha256:'bad'},stock,model),/integrity/);
 await assert.rejects(getSmkStock(model,async()=>({ok:true,arrayBuffer:async()=>fakeStock})),/does not match/);
 function device(text){const at=text.lastIndexOf('_');return {name:'Mock SMK',id:{text,model:text.slice(0,at),version:Number(text.slice(at+1))},link:{send(){starts++;return true},async close(){closed++}}};}
 let starts=0,closed=0,served=[],waits=[],initial=model+'_016',restart=built.product,fail=false,none=false;
 class Transport{
  async findAny(filter){const d=device(initial);return filter(d.id)?d:null;}
  async waitForAny(filter){const text=waits.shift()||restart,d=device(text);assert(filter(d.id));return none?null:d;}
  async serve(link,bytes,finish){served.push({bytes:bytes.slice(),finish});bytes[0]^=1;return {finished:!fail};}
 }
 const reset=()=>{starts=closed=0;served=[];waits=[];fail=none=false;restart=built.product;};
 async function install(handle=built.install,extra={}){return installSmkFirmware({},handle,()=>{},{Transport,...extra});}
 reset();initial=model+'_016';waits=['ota-'+model+'_016'];assert.equal(await install(),built.product);
 assert.deepEqual(served.map(s=>s.finish),[0xe0000000,0xf0000000]);assert.equal(starts,2);assert.equal(closed,3);
 const officialImage=served[1].bytes;assert.deepEqual(served[0].bytes,officialImage,'Each phase gets an unmodified immutable image');
 reset();initial='SMK-37_927';waits=['ota-SMK-37_927'];await install();assert.equal(served.length,1);assert.equal(served[0].bytes.length,1024+0x99000);
 const packet=served[0].bytes;
 reset();initial='ota-SMK-37_960';await install();assert.equal(starts,1);assert.deepEqual(served[0].bytes,packet,'Prior transport mutation must not affect retry');
 reset();initial='Doom-SMK-37_010';waits=['ota-Doom-SMK-37_010'];await install();assert.equal(served.length,1);
 reset();initial='SMK-37_921';await assert.rejects(install(),/older/);assert.equal(starts,0);
 reset();initial='FM-1_961';await assert.rejects(install(),/No SMK/);assert.equal(starts,0);
 reset();initial=model+'_015';await assert.rejects(install(),/016/);assert.equal(starts,0);
 reset();initial='ota-'+model+'_016';await install();assert.equal(served.length,1);assert.deepEqual(served[0].bytes,officialImage);
 reset();initial='ota-SMK-37_960';fail=true;await assert.rejects(install(),/interrupted/);assert.equal(served.length,1);
 reset();initial='ota-SMK-37_960';restart='SMK-37_927';await assert.rejects(install(),/Unexpected/);
 reset();initial='ota-SMK-37_960';none=true;await assert.rejects(install(),/restart was not confirmed/);
 reset();initial='ota-SMK-37_960';restart=model+'_016';assert.equal(await install(restore),model+'_016');
 if(i===1){
  reset();initial='ota-FM-1_016';await assert.rejects(install(),/confirmation/);assert.equal(starts,0);
  reset();initial='ota-FM-1_016';await install(built.install,{confirmGeneric:async(identity,selected)=>identity==='ota-FM-1_016'&&selected===model});
 }else assert(!smkIdentity(model)({model:'ota-FM-1'}));
 // Feed the real app packet to the C loader with a clearly synthetic tail.
 const flash=new Uint8Array(0x100000);flash.fill(0xa5);flash.set(before.flash);
 const trim=0xc1000;flash.set(new TextEncoder().encode('trimdat\0'),trim+548);
 const view=new DataView(flash.buffer);for(let k=0;k<37;k++)for(let j=0;j<5;j++)view.setUint16(trim+168+k*10+j*2,5+j*200,true);
 fs.writeFileSync(`build/smk37/candidate-${i}.fwsc`,built.bytes);fs.writeFileSync(`build/smk37/candidate-${i}.smkupdate`,packet);fs.writeFileSync(`build/smk37/synthetic-${i}.bin`,flash);
 console.log('PASS:',model,'exact cartridge packing, protected boot, stock hashes, immutable retries, factory/custom/Doom handoffs, interrupted/wrong restart and model-specific restore.');
}
