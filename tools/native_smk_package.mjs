// SPDX-License-Identifier: GPL-3.0-only
// Prepare only the site's pinned SMK Pro core or its verified factory restore.
import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {buildSmkPublicPackage,SMKPackage,smkLogicalImage} from '../web/vendor/smk37-package.js';
import {sha256hex} from '../web/vendor/fm1pkg.js';
import {prepareSmkRestore} from '../web/smk-firmware.js';
import {makeSmkPacket} from '../web/smk-format.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const [action,owner,out]=process.argv.slice(2);
if(!['install','restore'].includes(action)||!['public','owner'].includes(owner)||!out)throw Error('Invalid local installer request');
const stock=new Uint8Array(await fs.readFile(path.join(root,'build/smk37/stock-pro.fwsc')));
await prepareSmkRestore(stock,'SMK-37 Pro'); // Pins model, SHA and protected layout.
let image,packet,expected;
if(action==='restore'){
 const parsed=new SMKPackage(stock);image=smkLogicalImage(stock).raw;
 packet=makeSmkPacket(parsed.flash,'SMK-37_016','M-VAVE 016');expected='SMK-37 Pro_016';
}else{
 const manifest=JSON.parse(await fs.readFile(path.join(root,'web/firmware/nesynth.json'),'utf8'));
 let release=manifest.models['smk-pro'],appFile=path.join(root,'web',release.file);
 if(owner==='owner'){
  const report=JSON.parse(await fs.readFile(path.join(root,'build/bluetooth-loader/report.json'),'utf8'));
  if(!report.private_device_calibration||report.product!=='SMK-37_969')throw Error('Invalid owner core');
  release={product:report.product,bytes:report.app_bytes,sha256:report.sha256};
  appFile=path.join(root,'build/bluetooth-loader/SMK969-BLUETOOTH-RAW.bin');
 }
 const app=new Uint8Array(await fs.readFile(appFile));
 if(manifest.version!==969||release.product!=='SMK-37_969'||app.length!==release.bytes||app.length>216800||await sha256hex(app)!==release.sha256)throw Error('Core verification failed');
 const built=await buildSmkPublicPackage(stock,app,{...release,public:true,board:'SMK-37 Pro',version:'NESYNTH SMK 969'});
 image=built.image;packet=built.bytes;expected=release.product;
}
await fs.mkdir(out,{recursive:true});
await fs.writeFile(path.join(out,'image.bin'),image);await fs.writeFile(path.join(out,'packet.bin'),packet);
await fs.writeFile(path.join(out,'verified.json'),JSON.stringify({model:'SMK-37 Pro',expected,image:{bytes:image.length,sha256:await sha256hex(image)},packet:{bytes:packet.length,sha256:await sha256hex(packet)}}));
