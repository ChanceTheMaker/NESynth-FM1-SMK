// Local, unthrottled compatibility smoke run. No cartridge data is published.
// node tools/smoke_rom.mjs path.nes-or-zip [output-directory]
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {openCartridge} from '../web/zip.js';
import {normalizeCartridge} from '../web/cartridge.js';
const input=process.argv[2];if(!input)throw Error('Provide a local .nes or .zip path.');
const out=process.argv[3]||'build/rom-smoke';fs.mkdirSync(out,{recursive:true});
const entries=await openCartridge(new File([fs.readFileSync(input)],path.basename(input)));
if(entries.length!==1)throw Error('Smoke runner requires exactly one cartridge.');
const {bytes}=await normalizeCartridge(await entries[0].read());
const {instance}=await WebAssembly.instantiate(fs.readFileSync('web/nes.wasm'),{}),g=instance.exports;
new Uint8Array(g.memory.buffer,g.gp_upload(),bytes.length).set(bytes);
assert.equal(g.gp_load_upload(bytes.length),0,'Cartridge must load');
function snapshot(frame){
    const pixels=new Uint8Array(g.memory.buffer,g.gp_pixels(),256*240),palette=new Uint32Array(g.memory.buffer,g.np_palette(),64),rgb=Buffer.alloc(256*240*3);
    for(let i=0;i<pixels.length;i++){const c=palette[pixels[i]];rgb[i*3]=c>>16;rgb[i*3+1]=c>>8;rgb[i*3+2]=c;}
    fs.writeFileSync(path.join(out,`frame-${frame}.ppm`),Buffer.concat([Buffer.from('P6\n256 240\n255\n'),rgb]));
}
let sound=false;const start=performance.now();
for(let frame=1;frame<=3600;frame++){
    // Start through title and match intro, then exercise directional/action input.
    g.gp_buttons((frame>=400&&frame<410)||(frame>=900&&frame<910)||(frame>=1600&&frame<1610)||(frame>=2200&&frame<2210)?8:frame>=2500?(frame%120<60?1:32):0);
    assert.equal(g.gp_frame(),1,`Frame ${frame}`);assert.equal(g.gp_error(),0);
    const n=g.gp_audio_queued(),p=g.gp_render_audio(n);
    if(new Int16Array(g.memory.buffer,p,n*2).some(v=>v!==0))sound=true;
    if([120,399,899,1499,2400,3600].includes(frame))snapshot(frame);
}
console.log(JSON.stringify({mapper:(bytes[6]>>4)|(bytes[7]&240),frames:3600,sound,seconds:(performance.now()-start)/1000,output:out}));
