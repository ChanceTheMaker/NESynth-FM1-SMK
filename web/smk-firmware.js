// SPDX-License-Identifier: GPL-3.0-only
import {buildSmkPublicPackage,SMKPackage,smkLogicalImage} from './vendor/smk37-package.js';
import {STOCK_SOURCES} from './vendor/smk37-stock.js';
import {sha256hex} from './vendor/fm1pkg.js?v=20261013';
import {normalizeCartridge} from './cartridge.js';
import {validateHardwareRom,protectedAppHash} from './rom-firmware.js?v=961';
import {makeSmkPacket} from './smk-format.js';
import {sealSmkInstall} from './smk-install.js';
export const SMK_MODELS=Object.freeze({'smk-pro':'SMK-37 Pro','smk-elite':'SMK-37 Elite'});
const stockHashes=Object.freeze({'SMK-37 Pro':'2d98ce72530e71d617384963423820536f094501299e2cfec0a8d3c3fb6bcfb0','SMK-37 Elite':'b2ee38abfef3ae922e4eecdb8a551e14345300ae6f270265feacd33b6ef99cb6'});
export async function getSmkStock(model,fetcher=fetch){
 if(!stockHashes[model])throw Error('Choose an SMK model.');
 const response=await fetcher(STOCK_SOURCES[model],{signal:AbortSignal.timeout(60000),cache:'force-cache'});
 if(!response.ok)throw Error('The official SMK 016 firmware could not be downloaded. Retry when connected to the internet.');
 const bytes=new Uint8Array(await response.arrayBuffer());
 await checkStock(bytes,model);return bytes;
}
async function checkStock(bytes,model){
 if(!stockHashes[model]||await sha256hex(bytes)!==stockHashes[model])throw Error('Official firmware does not match the selected SMK model. Nothing was prepared.');
}
export async function prepareSmkFirmware(rom,template,meta,stock,model){
 rom=(await normalizeCartridge(rom)).bytes;validateHardwareRom(rom);await checkStock(stock,model);
 const offset=meta.cartridge_offset,capacity=meta.cartridge_capacity;
 if(meta.product!=='SMK-37_963'||meta.usb_updater!==true||meta.usb_protocol!=='smk37-ram-v2'||meta.bytes!==template.length||await sha256hex(template)!==meta.sha256||!Number.isInteger(offset)||offset<4||capacity!==524816||offset+capacity>template.length||!meta.mappers?.includes((rom[6]>>4)|(rom[7]&240)))throw Error('SMK template verification failed. Reload the site and retry.');
 if(rom.length>capacity)throw Error('This game is too large for device firmware. The limit is 512 KiB plus header and trainer. It can still run in the browser.');
 if(await protectedAppHash(template,offset,capacity)!==meta.protected_app_sha256)throw Error('SMK updater integrity check failed.');
 const app=template.slice();app.fill(255,offset,offset+capacity);app.set(rom,offset);
 if(await protectedAppHash(app,offset,capacity)!==meta.protected_app_sha256)throw Error('ROM packing changed the SMK updater.');
 const release={public:true,product:meta.product,version:'NESYNTH SMK 963',board:model,bytes:app.length,sha256:await sha256hex(app)};
 const built=await buildSmkPublicPackage(stock,app,release);
 const checked=new SMKPackage(built.factory);
 if(await sha256hex(checked.dec.subarray(checked.appOff,checked.appOff+app.length))!==release.sha256)throw Error('SMK application round-trip verification failed.');
 return {bytes:built.factory,product:meta.product,sha256:await sha256hex(built.factory),rom_sha256:await sha256hex(rom),usb_updater:true,install:await sealSmkInstall({...built,expectedProduct:meta.product})};
}
export async function prepareSmkRestore(stock,model){
 await checkStock(stock,model);const parsed=new SMKPackage(stock);
 if(parsed.appBase!==0x4000||parsed.appOff!==0x4120||parsed.flashSize!==0x9d000||parsed.chipKey!==0x980f)throw Error('Unsupported stock SMK layout.');
 const product='SMK-37_016',bytes=makeSmkPacket(parsed.flash,product,'M-VAVE 016');
 return sealSmkInstall({bytes,image:smkLogicalImage(stock).raw,stockModel:model,format:2,product,expectedProduct:model+'_016'});
}
