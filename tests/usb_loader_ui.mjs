// Full USB page connection through simulated Web MIDI; Bluetooth is absent.
import assert from 'node:assert/strict';
import {pack7} from '../web/vendor/fm1ota.js';
class Element{constructor(){this.children=[];this.value='';this.classList={add(){},remove(){}};}append(...c){this.children.push(...c);}replaceChildren(...c){this.children=c;}addEventListener(){}}
const elements=new Map();globalThis.document={querySelector:s=>{if(!elements.has(s))elements.set(s,new Element());return elements.get(s);},querySelectorAll:()=>[],createElement:()=>new Element()};
globalThis.setInterval=()=>{};globalThis.confirm=()=>true;
const listeners=new Set();let mode=0,command=0,requests=0,handoffs=0,version=968;
const input={name:'SMK MIDI',state:'connected',connection:'closed',open:async()=>{input.connection='open';},close:async()=>{input.connection='closed';},addEventListener:(t,f)=>listeners.add(f),removeEventListener:(t,f)=>listeners.delete(f)};
function emit(data){queueMicrotask(()=>{const ev={data:new Uint8Array(data)};listeners.forEach(f=>f(ev));input.onmidimessage?.(ev);});}
const output={name:'SMK MIDI',state:'connected',connection:'closed',open:async()=>{output.connection='open';},close:async()=>{output.connection='closed';},send:p=>{
 assert.notDeepEqual(Array.from(p),[0xf0,0x22,0x24,0x35,0x7f,0xf7],'Never start a firmware update');
 if(p[1]===0){const id=new Uint8Array(34);id.set([0,0x59,0x11]);id.set(new TextEncoder().encode('SMK-37_'+version),6);emit([0xf0,...pack7(id),0xf7]);return;}
 if(p[3]===76){if(p[4]===4){mode=4;handoffs++;return;}emit([0xf0,0x7d,78,76,0,mode,...new Array(29).fill(0),0xf7]);return;}
 assert.equal(p[3],85);let raw=[];
 if(p[6]===1){command=p[7]|p[8]<<7;}
 else{const words=command===6?[0x324c504e,2,0,401408,401408,262672,262672,32,0,0]:[0x314c504e,0,0,0,262672];const b=new Uint8Array(words.length*4),v=new DataView(b.buffer);words.forEach((n,i)=>v.setUint32(i*4,n,true));raw=b;}
 const response=[0xf0,0x7d,78,85,p[4],p[5],0];for(const b of raw)response.push(b&127,b>>7);emit([...response,0xf7]);
}};
const access={sysexEnabled:true,inputs:new Map([['i',input]]),outputs:new Map([['o',output]]),addEventListener(){},removeEventListener(){}};
Object.defineProperty(globalThis,'navigator',{value:{requestMIDIAccess:async options=>{assert(options.sysex);requests++;return access;}},configurable:true});
await import('../web/bluetooth-loader.js');document.querySelector('#transport').value='usb';document.querySelector('#transport').onchange();
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB connected');assert.equal(document.querySelector('#library').hidden,false);assert.equal(handoffs,1);assert.equal(requests,1);
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB disconnected');
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'USB connected');assert.equal(handoffs,1,'Already in USB mode: no extra reboot');
await document.querySelector('#connect').onclick();version=967;await document.querySelector('#connect').onclick();assert.match(document.querySelector('#status').textContent,/968/);assert.equal(handoffs,1);
console.log('PASS: real USB page handlers, MIDI permission, identity/mode discovery, radio-off handoff, shared library, reconnect and old-firmware rejection with Bluetooth absent');
