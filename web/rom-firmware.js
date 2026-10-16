import {Package,sha256hex,logicalImage} from './vendor/fm1pkg.js?v=20261013';
import {prepareInstall} from './install-firmware.js?v=20261013';
import {normalizeCartridge} from './cartridge.js?v=20261016-headers';
export async function protectedAppHash(app,offset,capacity){
 const code=new Uint8Array(app.length-capacity);code.set(app.subarray(0,offset));code.set(app.subarray(offset+capacity),offset);return sha256hex(code);
}
export function validateHardwareRom(rom){
 if(!(rom instanceof Uint8Array)||rom.length<16)throw Error('File is shorter than an iNES header.');
 if(rom[0]!==78||rom[1]!==69||rom[2]!==83||rom[3]!==26||(rom[7]&15)||rom.slice(10,16).some(x=>x))throw Error('Use a standard iNES 1.0 cartridge.');
 const mapper=(rom[6]>>4)|(rom[7]&240);
 if(![0,1,2,3,4,7,9,11].includes(mapper))throw Error('Unsupported mapper '+mapper+'. NESynth supports mappers 0 (NROM), 1 (MMC1), 2 (UxROM), 3 (CNROM), 4 (MMC3), 7 (AxROM), 9 (MMC2) and 11 (Color Dreams).');
 if(rom[9]&1)throw Error('PAL cartridges are not supported yet.');
 if(rom[8]>1)throw Error('Cartridge requests unsupported RAM.');
 if(rom.length!==16+((rom[6]&4)?512:0)+rom[4]*16384+rom[5]*8192)throw Error('Cartridge size does not match its header.');
 if(mapper===0&&(![1,2].includes(rom[4])||rom[5]>1))throw Error('Invalid NROM cartridge size.');
 if(mapper===2&&(![2,4,8,16].includes(rom[4])||rom[5]||(rom[6]&8)))throw Error('Unsupported UxROM cartridge size or mirroring.');
 if(mapper===7&&(![2,4,8,16].includes(rom[4])||rom[5]||(rom[6]&8)))throw Error('Unsupported AxROM cartridge size or mirroring.');
 if(mapper===3&&(![1,2].includes(rom[4])||![1,2,4].includes(rom[5])||(rom[6]&8)))throw Error('Unsupported CNROM cartridge size or mirroring.');
 if(mapper===4&&(![2,4,8,16,32].includes(rom[4])||![0,1,2,4,8,16,32].includes(rom[5])))throw Error('Unsupported MMC3 cartridge size.');
 if(mapper===11&&(![2,4,8].includes(rom[4])||![1,2,4,8,16].includes(rom[5])||(rom[6]&8)))throw Error('Unsupported Color Dreams cartridge size or mirroring.');
 if(mapper===1&&(![1,2,4,8,16].includes(rom[4])||![0,1,2,4,8,16].includes(rom[5])||(rom[6]&8)))throw Error('Unsupported MMC1 cartridge size or mirroring.');
 if(mapper===9&&(![2,4,8].includes(rom[4])||![1,2,4,8,16].includes(rom[5])||(rom[6]&8)))throw Error('Unsupported MMC2 cartridge size or mirroring.');
}
export async function prepareFirmware(rom,template,metadata){
 rom=(await normalizeCartridge(rom)).bytes;
 validateHardwareRom(rom);
 if(await sha256hex(template)!==metadata.sha256)throw Error('Firmware template failed its integrity check. Reload and retry.');
 const pkg=new Package(template),offset=metadata.cartridge_offset,capacity=metadata.cartridge_capacity;
 if(pkg.product!==metadata.product||!Number.isInteger(offset)||offset<0||capacity!==524816||offset+capacity>pkg.app.size)throw Error('Unsupported firmware template layout.');
 if(rom.length>capacity)throw Error('This game is too large for device firmware. The cartridge limit is 512 KiB plus its header and trainer. You can still play it in the browser.');
 if(!metadata.mappers?.includes((rom[6]>>4)|(rom[7]&240)))throw Error('The device template does not support this mapper. Reload the site and prepare again.');
 const app=pkg.dec.slice(pkg.appOff,pkg.appOff+pkg.app.size);
 const loader=pkg.entries.find(e=>e.type===100&&e.name==='ota.bin');
 if(metadata.usb_updater!==true||metadata.usb_protocol!=='fm1-ota-v1'||!loader||
    await protectedAppHash(app,offset,capacity)!==metadata.protected_app_sha256||
    await sha256hex(template.subarray(loader.dataOff+pkg.skew,loader.dataOff+pkg.skew+loader.size))!==metadata.loader_sha256)
  throw Error('USB updater code or loader failed verification. Nothing was prepared.');
 app.fill(255,offset,offset+capacity);app.set(rom,offset);pkg.setApp(app);
 const bytes=pkg.serialize(),check=new Package(bytes);
 if(check.product!==metadata.product)throw Error('Firmware identity mismatch.');
 const placed=check.dec.subarray(check.appOff+offset,check.appOff+offset+rom.length);
 if(await sha256hex(placed)!==await sha256hex(rom))throw Error('ROM verification failed.');
 const after=check.dec.subarray(check.appOff,check.appOff+check.app.size);
 if(await protectedAppHash(after,offset,capacity)!==metadata.protected_app_sha256||
    await sha256hex(bytes.subarray(loader.dataOff+pkg.skew,loader.dataOff+pkg.skew+loader.size))!==metadata.loader_sha256)
  throw Error('ROM packing changed the USB updater. Nothing was prepared.');
 const sha256=await sha256hex(bytes),install=await prepareInstall(bytes,{product:metadata.product,sha256});
 return {bytes,image:logicalImage(bytes),product:metadata.product,sha256,rom_sha256:await sha256hex(rom),install,usb_updater:true};
}
