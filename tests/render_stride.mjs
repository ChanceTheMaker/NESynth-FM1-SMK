// Compare every audio sample and every presented frame across render cadences.
// Optional local ROM/ZIP argument exercises a private game without publishing it.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {openCartridge} from '../web/zip.js';
const filename=process.argv[2]||'web/diagnostic.nes';
const entries=await openCartridge(new File([fs.readFileSync(filename)],path.basename(filename)));
assert.equal(entries.length,1);
const rom=await entries[0].read(),wasm=fs.readFileSync('web/nes.wasm');
async function core(stride){
  const {instance}=await WebAssembly.instantiate(wasm,{}),g=instance.exports;
  new Uint8Array(g.memory.buffer,g.gp_upload(),rom.length).set(rom);
  assert.equal(g.gp_load_upload(rom.length),0);g.np_render_stride(stride);return g;
}
const strides=[1,2,3,6,0],cores=await Promise.all(strides.map(core));
let previousAdaptive=0;const adaptiveGaps=new Set();
for(let frame=1;frame<=3600;frame++){
  const buttons=[400,900,1600,2200].some(n=>frame>=n&&frame<n+10)?8:frame>=2500?(frame%120<60?1:32):0;
  const results=cores.map(g=>{
    g.gp_buttons(buttons);assert.equal(g.gp_frame(),1);assert.equal(g.gp_error(),0);
    // Alternate a healthy buffer with starvation to exercise adaptive changes.
    const n=Math.max(0,g.gp_audio_queued()-(frame%100<50?2600:0)),p=g.gp_render_audio(n);
    return {audio:new Uint8Array(g.memory.buffer,p,n*4),pixels:new Uint8Array(g.memory.buffer,g.gp_pixels(),256*240)};
  });
  for(let i=1;i<cores.length;i++){
    assert.deepEqual(results[i].audio,results[0].audio,`audio frame ${frame}, stride ${strides[i]}`);
    if(cores[i].np_video_frame()===frame)assert.deepEqual(results[i].pixels,results[0].pixels,`pixels frame ${frame}, stride ${strides[i]}`);
  }
  if(cores[4].np_video_frame()===frame){if(previousAdaptive)adaptiveGaps.add(frame-previousAdaptive);previousAdaptive=frame;}
}
assert(adaptiveGaps.has(3)&&adaptiveGaps.has(6),'Adaptive mode must use both cadence limits');
console.log('PASS: 3,600 frames; strides 1/2/3/6 and adaptive mode produce identical audio and identical presented frames.');
