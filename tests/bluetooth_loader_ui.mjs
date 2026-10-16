// Exercise actual page handlers with a simulated GATT connection; no radio access.
import assert from 'node:assert/strict';
import {setupDom,dropChecks,fileEvent} from './loader_ui_helpers.mjs';
setupDom();
let beat;globalThis.setInterval=f=>{beat=f;};globalThis.confirm=()=>true;
let command=0,fail=false,readGate=null,reads=0;const listeners=[];
const device={addEventListener:(event,fn)=>listeners.push(fn),gatt:{connected:false,connect:async()=>{device.gatt.connected=true;return {getPrimaryService:async()=>({getCharacteristic:async()=>channel})};},disconnect:()=>{device.gatt.connected=false;listeners.forEach(fn=>fn());}}};
const channel={service:{device},properties:{writeWithoutResponse:true},writeValueWithResponse:async bytes=>{command=bytes[0];},readValue:async()=>{
 reads++;if(readGate)await readGate;if(fail)throw Error('Temporary read failure');
 const words=command===6?[0x324c504e,2,0,401408,401408,262672,262672,32,0,0]:[0x314c504e,0,0,0,262672];
 const a=new Uint8Array(words.length*4),v=new DataView(a.buffer);words.forEach((n,i)=>v.setUint32(i*4,n,true));return v;
}};
Object.defineProperty(globalThis,'navigator',{value:{bluetooth:{requestDevice:async()=>device}},configurable:true});
await import('../web/bluetooth-loader.js');
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth connected');assert.equal(document.querySelector('#library').hidden,false);assert.match(document.querySelector('#space').textContent,/392 KiB/);
await dropChecks(beat);
const before=reads;fileEvent('dragenter');await beat();assert.equal(reads,before,'No polling during drag');fileEvent('drop');
document.hidden=true;await beat();assert.equal(reads,before);document.hidden=false;
fail=true;await beat();assert(device.gatt.connected);assert.equal(document.querySelector('#connection').textContent,'Bluetooth connection unconfirmed');assert.equal(document.querySelector('#library').hidden,false);
fail=false;await beat();assert.equal(document.querySelector('#connection').textContent,'Bluetooth connected');
let release;readGate=new Promise(r=>{release=r;});const pending=beat();
const raw=new Uint8Array(16400);raw.set([78,69,83,26,1,0]);fileEvent('drop',{name:'During check.nes',size:raw.length,arrayBuffer:async()=>raw.buffer});
await new Promise(r=>setImmediate(r));assert.match(document.querySelector('#name').textContent,/During check/);assert(document.querySelector('#send').disabled);
release();await pending;readGate=null;assert(!document.querySelector('#send').disabled);
device.gatt.disconnect();assert.equal(document.querySelector('#connection').textContent,'Bluetooth disconnected');assert(document.querySelector('#send').disabled);assert(document.querySelector('#library').hidden);
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth connected');await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth disconnected');
console.log('PASS: Bluetooth drop/import, check overlap, transient failure recovery, physical disconnect and reconnect');
