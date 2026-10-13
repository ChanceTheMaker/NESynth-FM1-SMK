import {validateStockPackage} from './vendor/fm1pkg.js?v=20261013';
import {prepareInstall,installFirmware} from './install-firmware.js?v=20261013';
export async function downloadOriginal(fetcher=fetch){
 const response=await fetcher('firmware/fm1-original.fwsc',{cache:'no-store',signal:AbortSignal.timeout(20000)});
 if(!response.ok)throw Error('The official firmware download failed. Please try again.');
 const bytes=new Uint8Array(await response.arrayBuffer());
 await validateStockPackage(bytes);
 return bytes;
}
export async function restoreLatest(access,log=()=>{},options={},fetcher=fetch){
 log('Downloading and verifying M-VAVE FM-1 V15...');
 return restoreOriginal(access,await downloadOriginal(fetcher),log,options);
}
export async function restoreOriginal(access,bytes,log=()=>{},options={}){
 // Validate the complete official package before the updater can access MIDI.
 const verified=await validateStockPackage(bytes),pkg=await prepareInstall(bytes,verified);
 return installFirmware(access,pkg,log,options);
}
