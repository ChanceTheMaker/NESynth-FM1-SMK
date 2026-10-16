// Full USB page connection through simulated Web MIDI; Bluetooth is absent.
import assert from 'node:assert/strict';
import {pack7} from '../web/vendor/fm1ota.js';
import {setupDom,dropChecks,fileEvent} from './loader_ui_helpers.mjs';
setupDom();
const fm1=process.argv.includes('--fm1'),model=fm1?'FM-1':'SMK-37',minimum=fm1?969:968;
let beat;globalThis.setInterval=f=>{beat=f;};globalThis.confirm=()=>true;
const listeners=new Set();let mode=0,command=0,requests=0,handoffs=0,version=minimum,timeout=false,gameCommands=0;
const input={name:model+' MIDI',state:'connected',connection:'closed',open:async()=>{input.connection='open';},close:async()=>{input.connection='closed';},addEventListener:(t,f)=>listeners.add(f),removeEventListener:(t,f)=>listeners.delete(f)};
function emit(data){queueMicrotask(()=>{const ev={data:new Uint8Array(data)};listeners.forEach(f=>f(ev));input.onmidimessage?.(ev);});}
const output={name:model+' MIDI',state:'connected',connection:'closed',open:async()=>{output.connection='open';},close:async()=>{output.connection='closed';},send:p=>{
 assert.notDeepEqual(Array.from(p),[0xf0,0x22,0x24,0x35,0x7f,0xf7],'Never start a firmware update');
 if(p[1]===0){const id=new Uint8Array(34);id.set([0,0x59,0x11]);id.set(new TextEncoder().encode(model+'_'+version),6);emit([0xf0,...pack7(id),0xf7]);return;}
 if(p[3]===76){if(p[4]===4){mode=4;handoffs++;return;}emit([0xf0,0x7d,78,76,0,mode,...new Array(29).fill(0),0xf7]);return;}
 assert.equal(p[3],85);if(timeout)return;let raw=[];
 if(p[6]===1){command=p[7]|p[8]<<7;if([1,2,3,8,9].includes(command))gameCommands++;}
 else{const words=command===6?[0x324c504e,2,0,401408,401408,262672,262672,32,0,0]:[0x314c504e,0,0,0,262672];const b=new Uint8Array(words.length*4),v=new DataView(b.buffer);words.forEach((n,i)=>v.setUint32(i*4,n,true));raw=b;}
 const response=[0xf0,0x7d,78,85,p[4],p[5],0];for(const b of raw)response.push(b&127,b>>7);emit([...response,0xf7]);
}};
const access={sysexEnabled:true,inputs:new Map([['i',input]]),outputs:new Map([['o',output]]),addEventListener(){},removeEventListener(){}};
Object.defineProperty(globalThis,'navigator',{value:{requestMIDIAccess:async options=>{assert(options.sysex);requests++;return access;}},configurable:true});
await import('../web/bluetooth-loader.js');document.querySelector('#transport').value='usb';document.querySelector('#transport').onchange();
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB connected');assert.equal(document.querySelector('#library').hidden,false);assert.equal(handoffs,1);assert.equal(requests,1);
await dropChecks(beat);assert.equal(gameCommands,0,'Selecting files never sends game commands');
timeout=true;const pending=beat();
const raw=new Uint8Array(16400);raw.set([78,69,83,26,1,0]);fileEvent('drop',{name:'USB during check.nes',size:raw.length,arrayBuffer:async()=>raw.buffer});
await new Promise(r=>setImmediate(r));assert.match(document.querySelector('#name').textContent,/USB during check/);
await pending;assert.equal(document.querySelector('#connection').textContent,'USB connection unconfirmed');assert.equal(input.connection,'open');assert.equal(gameCommands,0);
timeout=false;await beat();assert.equal(document.querySelector('#connection').textContent,'USB connected');assert(!document.querySelector('#send').disabled);
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB disconnected');
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB connected');assert.equal(handoffs,1,'Already in USB mode: no extra reboot');
await document.querySelector('#connect').onclick();version=minimum-1;await document.querySelector('#connect').onclick();assert.match(document.querySelector('#status').textContent,new RegExp(String(minimum)));assert.equal(handoffs,1);
console.log('PASS: real USB page handlers, MIDI permission, identity/mode discovery, radio-off handoff, shared library, reconnect and old-firmware rejection with Bluetooth absent');
