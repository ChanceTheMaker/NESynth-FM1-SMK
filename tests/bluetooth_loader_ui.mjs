// Exercise actual page handlers with a simulated GATT connection; no radio access.
import assert from 'node:assert/strict';
class Element {constructor(){this.children=[];this.disabled=false;this.hidden=false;this.textContent='';this.classList={add(){},remove(){}};}append(...c){this.children.push(...c);}replaceChildren(...c){this.children=c;}addEventListener(){} }
const elements=new Map();globalThis.document={querySelector:s=>{if(!elements.has(s))elements.set(s,new Element());return elements.get(s);},querySelectorAll:()=>document.querySelector('#games').children.flatMap(r=>r.children.slice(1)),createElement:()=>new Element()};
let beat;globalThis.setInterval=f=>{beat=f;};globalThis.confirm=()=>true;
let command=0,fail=false;const listeners=[];
const device={addEventListener:(event,fn)=>listeners.push(fn),gatt:{connected:false,connect:async()=>{device.gatt.connected=true;return {getPrimaryService:async()=>({getCharacteristic:async()=>channel})};},disconnect:()=>{device.gatt.connected=false;listeners.forEach(fn=>fn());}}};
const channel={service:{device},properties:{writeWithoutResponse:true},writeValueWithResponse:async bytes=>{command=bytes[0];},readValue:async()=>{
 if(fail)throw Error('Radio off');
 const words=command===6?[0x324c504e,2,0,401408,401408,262672,262672,32,0,0]:[0x314c504e,0,0,0,262672];
 const a=new Uint8Array(words.length*4),v=new DataView(a.buffer);words.forEach((n,i)=>v.setUint32(i*4,n,true));return v;
}};
Object.defineProperty(globalThis,'navigator',{value:{bluetooth:{requestDevice:async()=>device}},configurable:true});
await import('../web/bluetooth-loader.js');
await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth connected');assert.equal(document.querySelector('#library').hidden,false);assert.match(document.querySelector('#space').textContent,/392 KiB/);
fail=true;await beat();assert.equal(document.querySelector('#connection').textContent,'Bluetooth disconnected');assert.equal(document.querySelector('#send').disabled,true);assert.equal(document.querySelector('#library').hidden,true);
fail=false;await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth connected');await document.querySelector('#connect').onclick();assert.equal(document.querySelector('#connection').textContent,'Bluetooth disconnected');
console.log('PASS: actual browser connect, library display, heartbeat detects radio loss, disconnect disables send, and reconnect');
