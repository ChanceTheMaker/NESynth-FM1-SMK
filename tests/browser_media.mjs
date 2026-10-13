import assert from 'node:assert/strict';
import fs from 'node:fs';
import {paintFrame,BrowserAudio} from '../web/media.js';
// ImageData uses clamped bytes, unlike Buffer/Uint8Array: packed RGB must be masked.
const palette=new Uint32Array(64);palette.set([0x666666,0x002a88,0xfe8170,0x388700,0xfffeff]);
const rgba=new Uint8ClampedArray(20);paintFrame(new Uint8Array([0,1,2,3,4]),palette,rgba);
assert.deepEqual([...rgba],[102,102,102,255,0,42,136,255,254,129,112,255,56,135,0,255,255,254,255,255]);
class Context{
 constructor(){this.state='suspended';this.currentTime=12;this.destination={};this.nodes=[];this.resumes=0;}
 async resume(){this.resumes++;if(this.fail)throw Error('blocked');this.state='running';this.onstatechange?.();}
 createBuffer(channels,count,rate){assert.equal(channels,1);assert.equal(rate,44100);return {samples:new Float32Array(count),getChannelData(){return this.samples;}};}
 createBufferSource(){const node={connect(dest){this.destination=dest;},start(at){this.at=at;},stop(){this.stopped=true;},disconnect(){this.disconnected=true;}};this.nodes.push(node);return node;}
}
const states=[],audio=new BrowserAudio(state=>states.push(state),Context);
const enabling=audio.enable();assert.equal(audio.context.resumes,1,'Resume must begin synchronously inside the user gesture');await enabling;
const {instance}=await WebAssembly.instantiate(fs.readFileSync('web/nes.wasm'),{}),core=instance.exports;
const rom=fs.readFileSync('web/diagnostic.nes');new Uint8Array(core.memory.buffer,core.gp_upload(),rom.length).set(rom);assert.equal(core.gp_load_upload(rom.length),0);
let audible=false;
for(let i=0;i<20;i++){
 assert.equal(core.gp_frame(),1);const count=core.gp_audio_queued(),ptr=core.gp_render_audio(count);
 audio.queue(new Int16Array(core.memory.buffer,ptr,count*2),count);
 const node=audio.context.nodes.at(-1);assert.equal(node.destination,audio.context.destination);
 audible||=node.buffer.samples.some(v=>Math.abs(v)>.001);
 assert(node.buffer.samples.every(v=>Number.isFinite(v)&&v>=-1&&v<=1));
}
assert(audible,'Diagnostic PCM must reach the connected browser audio buffer');
assert.equal(audio.context.nodes[0].at,12.025);
assert(audio.context.nodes.slice(1).every((n,i)=>Math.abs(n.at-audio.context.nodes[i].at-audio.context.nodes[i].buffer.samples.length/44100)<1e-10));
audio.context.state='suspended';audio.context.onstatechange();assert.equal(states.at(-1),false);assert(audio.context.nodes.every(n=>n.stopped));
await audio.enable();assert.equal(audio.context.resumes,2);assert.equal(states.at(-1),true);
audio.context.state='suspended';audio.context.fail=true;await assert.rejects(audio.enable(),/blocked/);audio.context.fail=false;await audio.enable();assert.equal(audio.running,true);
console.log('PASS: exact clamped RGB channels, audible WASM PCM routing, sample rate, continuous scheduling, suspension and resume retry.');
