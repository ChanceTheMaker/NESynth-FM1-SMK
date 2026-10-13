import assert from 'node:assert/strict';
import fs from 'node:fs';
import {performance} from 'node:perf_hooks';
const rom=new Uint8Array(fs.readFileSync('web/diagnostic.nes'));
const {instance}=await WebAssembly.instantiate(fs.readFileSync('web/nes.wasm'),{}),g=instance.exports;
const memory=new Uint8Array(g.memory.buffer),upload=g.gp_upload();
function load(bytes){memory.set(bytes,upload);return g.gp_load_upload(bytes.length);}
function step(){assert.equal(g.gp_frame(),1);assert.equal(g.gp_error(),0);g.gp_render_audio(g.gp_audio_queued());}
function hash(){let h=2166136261;for(const b of memory.subarray(g.gp_pixels(),g.gp_pixels()+256*240))h=Math.imul(h^b,16777619);return h>>>0;}
assert.equal(load(rom),0);for(let i=0;i<10;i++)step();
const host=JSON.parse(fs.readFileSync('build/host-benchmark.json'));
assert.equal(hash(),host.diagnostic_frame10_fnv1a,'Host and WASM must render the same diagnostic frame');
const normal=hash();g.gp_buttons(16);for(let i=0;i<12;i++)step();g.gp_buttons(0);step();assert.notEqual(hash(),normal);
let seed=0x12345678;const rand=()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return seed>>>0;};
// Invalid headers must reject cleanly, without depending on previous cartridge state.
for(let i=0;i<256;i++){const bytes=rom.slice();bytes[rand()%16]=rand()&255;const rc=load(bytes);assert.ok(rc>=0&&rc<=8);if(!rc)step();}
// Random NROM programs exercise bounded stepping and PPU/APU write paths in WASM.
// Trapping or escaping linear memory fails the test; a controlled core error is allowed.
for(let i=0;i<64;i++){
    const bytes=rom.slice();for(let j=16;j<16400;j++)bytes[j]=rand()&255;
    bytes[16+0x3ffc]=0;bytes[16+0x3ffd]=0x80;
    assert.equal(load(bytes),0);for(let j=0;j<3&&!g.gp_error();j++){g.gp_frame();g.gp_render_audio(g.gp_audio_queued());}
}
assert.equal(load(rom),0);const start=performance.now();for(let i=0;i<1800;i++)step();const elapsed=performance.now()-start;
assert.equal(g.gp_frame_count(),1800);assert.equal(g.np_audio_samples(),Math.floor(1800*44100*59561/3579545));
const report={frames:1800,seconds:elapsed/1000,fps:1800000/elapsed,headerCases:256,randomPrograms:64,hostFrameMatches:true,hardwareFps:null};
fs.writeFileSync('build/wasm-benchmark.json',JSON.stringify(report,null,2)+'\n');console.log(report);
