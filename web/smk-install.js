// SPDX-License-Identifier: GPL-3.0-only
// Felucca/Sloop/Doom SMK handoffs, with immutable packages and exact restart checks.
import {Updater} from './vendor/fm1ota.js?v=973';
import {sha256hex} from './vendor/fm1pkg.js?v=20261013';
import {crc32,HEADER,LO,HI} from './smk-format.js';
import {reportProgress} from './update-progress.js?v=20261013';
const verified=new WeakMap(),START=[0xf0,0x22,0x24,0x35,0x7f,0xf7];
export const SMK_PORT_PATTERN=/smk|nesynth|doom|felucca|sloop|melodee|ota|composite|sinco|usb-midi/i;
const portable=id=>['SMK-37','ota-SMK-37','Doom-SMK-37','ota-Doom-SMK-37'].includes(id.model);
export const smkIdentity=model=>id=>portable(id)||id.model===model||id.model==='ota-'+model||(model==='SMK-37 Elite'&&id.model==='ota-FM-1');
export async function sealSmkInstall(pkg){
 if(!pkg||!['SMK-37 Pro','SMK-37 Elite'].includes(pkg.stockModel)||pkg.format!==2||!(pkg.image instanceof Uint8Array)||pkg.image.length<HI||pkg.bytes?.length!==HEADER+HI-LO)throw Error('Prepare a verified SMK package first.');
 const bytes=pkg.bytes.slice(),header=bytes.slice(0,HEADER),v=new DataView(header.buffer),str=(a,b)=>new TextDecoder().decode(header.subarray(a,b)).replace(/\0+$/,'');
 const checksum=v.getUint32(44,true);v.setUint32(44,0,true);
 if(str(0,16)!=='SMK37-SALT-USB1'||v.getUint32(16,true)!==2||v.getUint32(20,true)!==LO||v.getUint32(24,true)!==HI||v.getUint32(28,true)!==HI-LO||v.getUint32(32,true)!==0x3a04ad23||v.getUint32(36,true)!==0||crc32(header)!==checksum||crc32(bytes.subarray(HEADER))!==v.getUint32(40,true)||str(48,80)!==pkg.product||!/^SMK-37_\d{3}$/.test(pkg.product)||!str(80,112)||header.slice(112,128).some(x=>x)||header.slice(128+(HI-LO)/4096*4).some(x=>x))throw Error('Invalid SMK update packet.');
 for(let i=0;i<(HI-LO)/4096;i++)if(crc32(bytes.subarray(HEADER+i*4096,HEADER+(i+1)*4096))!==v.getUint32(128+i*4,true))throw Error('SMK sector checksum failed.');
 const expectedProduct=pkg.expectedProduct;
 if(expectedProduct!==pkg.product&&expectedProduct!==pkg.stockModel+'_016')throw Error('Invalid restart identity.');
 const handle=Object.freeze({product:pkg.product,expectedProduct,model:pkg.stockModel,sha256:await sha256hex(bytes)});
 verified.set(handle,{bytes,image:pkg.image.slice()});return handle;
}
export async function installSmkFirmware(access,handle,log=()=>{},{Transport=Updater,confirmGeneric}={}){
 if(!verified.has(handle))throw Error('Prepare a verified SMK package first.');
 const packet=verified.get(handle),up=new Transport(access,log,SMK_PORT_PATTERN),model=handle.model;
 let link;
 try{
  let dev=await up.findAny(smkIdentity(model));
  if(!dev)throw Error('No '+model+' replied. Connect only this instrument over USB and close other MIDI apps. Use official 016 or an updater-enabled SMK application.');
  link=dev.link;log('Connected: '+dev.id.text+' on '+dev.name);
  const custom=portable(dev.id);
  if(custom){
   const minimum=dev.id.model.includes('Doom')?7:922;
   if(!Number.isInteger(dev.id.version)||dev.id.version<minimum)throw Error('This older SMK updater needs a recovery-adapter migration. No update was started.');
   if(!dev.id.model.startsWith('ota-')){
    const loaderModel='ota-'+dev.id.model;reportProgress({phase:'loader'});
    if(!link.send(START))throw Error('SMK disconnected before loader handoff.');
    await link.close();link=null;dev=await up.waitForAny(id=>id.model===loaderModel,30000);
    if(!dev)throw Error('SMK RAM loader did not appear. Keep power connected and retry.');
    link=dev.link;
   }
  }else if(dev.id.model===model){
   if(dev.id.version!==16)throw Error('First installation requires official '+model+' 016 firmware. No update was started.');
   reportProgress({phase:'loader'});log('Preparing the official SMK loader. Keep power connected.');
   if(!link.send(START))throw Error('SMK disconnected before loader preparation.');
   const first=await up.serve(link,packet.image.slice(),0xe0000000,15000);
   await link.close();link=null;
   if(!first.finished)throw Error('Loader preparation stopped. Keep power connected; no automatic retry was attempted.');
   dev=await up.waitForAny(id=>id.model==='ota-'+model||(model==='SMK-37 Elite'&&id.model==='ota-FM-1'),30000);
   if(!dev)throw Error('Official loader did not appear. Keep the SMK powered and use Retry install / resume.');
   link=dev.link;
  }
  if(dev.id.model==='ota-FM-1'&&(!confirmGeneric||!await confirmGeneric(dev.id.text,model)))throw Error('SMK model confirmation cancelled. No application programming started.');
  log('Installing '+handle.product+'. Keep power and USB connected.');
  if(!link.send(START))throw Error('Loader disconnected before programming.');
  const result=await up.serve(link,(custom?packet.bytes:packet.image).slice(),0xf0000000,180000);
  await link.close();link=null;
  if(!result.finished)throw Error('Installation interrupted. Keep the SMK powered and use Retry install / resume. If power was lost during programming, recovery may require the adapter.');
  reportProgress({phase:'reboot'});log('Transfer verified. Checking the restarted firmware identity...');
  const back=await up.waitForAny(id=>id.model==='SMK-37'||id.model===model,40000);
  if(!back)throw Error('Transfer finished, but restart was not confirmed. Check the screen and USB connection.');
  link=back.link;
  if(back.id.text!==handle.expectedProduct)throw Error('Unexpected firmware after restart: '+back.id.text+'; expected '+handle.expectedProduct+'.');
  log('Installed and restart verified: '+back.id.text);return back.id.text;
 }finally{await link?.close();}
}
