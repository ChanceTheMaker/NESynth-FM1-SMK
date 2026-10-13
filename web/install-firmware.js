// SPDX-License-Identifier: GPL-3.0-only
// GameDude adaptation of the Sloop/Felucca transport and MusicCoach FM-1 handoff.
import {Updater} from './vendor/fm1ota.js?v=20261013';
import {Package,logicalImage,sha256hex} from './vendor/fm1pkg.js?v=20261013';
import {reportProgress} from './update-progress.js?v=20261013';
const verified=new WeakMap();
const START=[0xf0,0x22,0x24,0x35,0x7f,0xf7];
export const fm1Identity=id=>['FM-1','ota-FM-1','Doom-FM-1','ota-Doom-FM-1'].includes(id.model);
export async function prepareInstall(bytes,{product,sha256}){
 if(!/^FM-1_\d{3}$/.test(product)||await sha256hex(bytes)!==sha256)throw Error('Firmware integrity check failed. Nothing was installed.');
 const parsed=new Package(bytes);
 if(parsed.product!==product||parsed.appBase!==0x4000||parsed.appOff!==0x4120||parsed.flashSize!==0x93000)
  throw Error('Unsupported FM-1 firmware layout. Nothing was installed.');
 const pkg=Object.freeze({product,sha256});verified.set(pkg,logicalImage(bytes));return pkg;
}
export async function installFirmware(access,pkg,log=()=>{},{confirmGeneric,Transport=Updater}={}){
 if(!verified.has(pkg))throw Error('Prepare a verified firmware package first.');
 const image=verified.get(pkg).slice(),up=new Transport(access,log);let link;
 try{
  let dev=await up.findAny(fm1Identity);
  if(!dev)throw Error('No updater-enabled FM-1 found. Connect only the FM-1 using a data cable and close other MIDI apps. Older firmware without a USB updater needs the recovery adapter.');
  link=dev.link;log('Connected: '+dev.id.text+' on '+dev.name);
  // Some Elite loaders also call themselves ota-FM-1. Never infer the board
  // from that generic identity when resuming a separately started session.
  if(dev.id.model==='ota-FM-1'&&(!confirmGeneric||!await confirmGeneric(dev.id.text)))
   throw Error('FM-1 model confirmation cancelled. No programming started.');
  if(dev.id.model==='Doom-FM-1'){
   if(dev.id.version<7)throw Error('This Doom version needs the recovery adapter; its USB updater is not supported.');
   log('Entering the Doom FM-1 RAM updater.');
   if(!link.send(START))throw Error('FM-1 disconnected before handoff.');
   await link.close();link=null;
   dev=await up.waitForAny(id=>id.model==='ota-Doom-FM-1',30000);
   if(!dev)throw Error('Doom loader did not appear. Keep power connected and use Retry install / resume.');
   link=dev.link;
  }
  if(dev.id.model==='FM-1'){
   log('Preparing the update loader. Keep power and USB connected.');reportProgress({phase:'loader'});
   if(!link.send(START))throw Error('FM-1 disconnected before loader preparation.');
   const first=await up.serve(link,image,0xe0000000,15000);
   await link.close();link=null;
   if(!first.finished)throw Error('Loader preparation stopped. Keep power connected, save the log, and inspect the device before retrying.');
   dev=await up.waitForAny(id=>id.model==='ota-FM-1',30000);
   if(!dev)throw Error('Loader has not replied. Keep the FM-1 powered, reconnect USB, then use Retry install / resume.');
   link=dev.link;
  }else log('Active loader found; resuming without repeating preparation.');
  log('Installing '+pkg.product+'. Keep power and USB connected.');
  const result=await up.write(dev,image,()=>{});
  await link.close();link=null;
  if(!result.finished)throw Error('Transfer interrupted. Keep power connected and save the log, then use Retry install / resume. No write was retried automatically.');
  reportProgress({phase:'reboot'});log('Transfer finished. Waiting for the firmware restart identity...');
  const back=await up.waitForAny(id=>id.model==='FM-1',40000);
  if(!back)throw Error('Transfer finished, but restart was not confirmed. Check the screen and reconnect USB before retrying.');
  link=back.link;
  if(back.id.text!==pkg.product)throw Error('Unexpected firmware after restart: '+back.id.text+'; expected '+pkg.product+'.');
  log('Installed and restart verified: '+back.id.text+'.');return back.id.text;
 }finally{await link?.close();}
}
