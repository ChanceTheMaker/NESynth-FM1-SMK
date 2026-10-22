// SPDX-License-Identifier: GPL-3.0-only
import {buildFeluccaPackage,sha256hex} from './vendor/fm1pkg.js?v=20261013';
import {buildSmkPublicPackage} from './vendor/smk37-package.js';
import {SMK_MODELS,getSmkStock} from './smk-firmware.js?v=963';
import {prepareFirmwareFile} from './firmware-file.js?v=padlights971';

export async function loadNesynthFirmware(model,fetcher=fetch){
 if(model!=='fm1'&&!SMK_MODELS[model])throw Error('Choose your instrument.');
 async function download(url){
  const response=await fetcher(url,{cache:'no-store',signal:AbortSignal.timeout(60000)});
  if(!response.ok)throw Error('NESynth firmware download failed. Check your connection and retry.');
  return response;
 }
 const manifest=await (await download('firmware/nesynth.json')).json(),release=manifest.models?.[model];
 const product=model==='fm1'?'FM-1_969':'SMK-37_971';
 if(manifest.version!==969||release?.product!==product||!/^firmware\/[a-z0-9-]+\.bin$/.test(release?.file)||!Number.isInteger(release?.bytes)||release.bytes<4||release.bytes>(model==='fm1'?204512:216800)||!/^[a-f0-9]{64}$/.test(release?.sha256))throw Error('NESynth release information is invalid. Reload the site and retry.');
 const app=new Uint8Array(await (await download(release.file)).arrayBuffer());
 if(app.length!==release.bytes||await sha256hex(app)!==release.sha256)throw Error('NESynth firmware verification failed. Reload the site and retry.');
 let bytes;
 if(model==='fm1'){
  const stock=new Uint8Array(await (await download('firmware/fm1-original.fwsc')).arrayBuffer());
  bytes=await buildFeluccaPackage(stock,app,product);
 }else{
  const stock=await getSmkStock(SMK_MODELS[model],fetcher);
  const built=await buildSmkPublicPackage(stock,app,{public:true,board:SMK_MODELS[model],product,version:'NESYNTH SMK 971',bytes:app.length,sha256:release.sha256});
  bytes=built.factory;
 }
 return prepareFirmwareFile(bytes,model);
}
