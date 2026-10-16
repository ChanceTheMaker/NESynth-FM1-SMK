// SPDX-License-Identifier: GPL-3.0-only
import {Package,sha256hex} from './vendor/fm1pkg.js?v=20261013';
import {SMKPackage,smkLogicalImage} from './vendor/smk37-package.js';
import {prepareInstall} from './install-firmware.js?v=20261013';
import {makeSmkPacket} from './smk-format.js';
import {sealSmkInstall} from './smk-install.js';
import {SMK_MODELS} from './smk-firmware.js?v=963';

// User-selected firmware stays local. No cartridge is inserted into this file.
export async function prepareFirmwareFile(input,model){
 if(!(input instanceof Uint8Array)||input.length<960||input.length>2*1024*1024)throw Error('Choose a valid NESynth .fwsc firmware file (up to 2 MiB).');
 if(model!=='fm1'&&!SMK_MODELS[model])throw Error('Choose your instrument first.');
 const bytes=input.slice(),fm=model==='fm1',board=fm?'FM-1':SMK_MODELS[model];
 const parsed=fm?new Package(bytes):new SMKPackage(bytes);
 const match=parsed.product.match(/^(FM-1|SMK-37 Pro|SMK-37 Elite)_(9\d\d)$/);
 if(!match||match[1]!==board)throw Error('This firmware does not match '+board+'. Choose its NESynth .fwsc file.');
 if(Number(match[2])<969)throw Error('Choose NESynth library firmware 969 or newer for Bluetooth and USB game loading.');
 const product=(fm?'FM-1':'SMK-37')+'_'+match[2],sha256=await sha256hex(bytes);
 if(fm)return {product,sha256,install:await prepareInstall(bytes,{product,sha256})};
 if(parsed.appBase!==0x4000||parsed.appOff!==0x4120||parsed.flashSize!==0x9d000||parsed.chipKey!==0x980f)throw Error('Unsupported SMK firmware layout.');
 const packet=makeSmkPacket(parsed.flash,product,'NESYNTH SMK '+match[2]);
 return {product,sha256,install:await sealSmkInstall({bytes:packet,image:smkLogicalImage(bytes).raw,stockModel:board,format:2,product,expectedProduct:product})};
}
