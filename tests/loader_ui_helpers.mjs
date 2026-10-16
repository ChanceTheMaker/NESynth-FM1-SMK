// Minimal event-capable DOM for both real uploader entry points.
import assert from 'node:assert/strict';
export function setupDom(){
 class Element extends EventTarget{
  constructor(){super();this.children=[];this.value='';this.disabled=false;this.hidden=false;this.textContent='';this.classList={add(){},remove(){}};}
  append(...c){this.children.push(...c);}replaceChildren(...c){this.children=c;}
  contains(target){return target===this||this.children.some(c=>c.contains(target));}
 }
 const elements=new Map(),doc=new EventTarget();
 Object.assign(doc,{hidden:false,hasFocus:()=>true,querySelector:s=>{if(!elements.has(s))elements.set(s,new Element());return elements.get(s);},querySelectorAll:()=>doc.querySelector('#games').children.flatMap(r=>r.children.slice(1)),createElement:()=>new Element()});
 globalThis.document=doc;globalThis.Option=class extends Element{constructor(name,value){super();this.textContent=name;this.value=value;}};
 return doc;
}
export function fileEvent(type,file,target=document.querySelector('#drop')){
 const event=new Event(type,{cancelable:true});Object.defineProperty(event,'target',{value:target});
 event.dataTransfer={types:['Files'],files:file?[file]:[]};document.dispatchEvent(event);return event;
}
const flush=()=>new Promise(r=>setImmediate(r));
export async function dropChecks(beat){
 const name=document.querySelector('#name'),send=document.querySelector('#send'),connection=document.querySelector('#connection');
 const label=connection.textContent;
 const raw=new Uint8Array(16400);raw.set([78,69,83,26,1,0]);
 let release;const file={name:'Drop test.nes',size:raw.length,arrayBuffer:()=>new Promise(r=>{release=r;})};
 assert(fileEvent('dragover').defaultPrevented);
 const child=document.createElement('p');document.querySelector('#drop').append(child);
 assert(fileEvent('drop',file,child).defaultPrevented);assert(send.disabled);
 await beat();assert.equal(connection.textContent,label);
 // Competing drops cannot replace an import already in progress.
 fileEvent('drop',{name:'ignored.nes',size:0,arrayBuffer:()=>{throw Error('Concurrent import');}});
 release(raw.buffer);await flush();assert.match(name.textContent,/Drop test.nes/);assert(!send.disabled);assert.equal(connection.textContent,label);
 assert(fileEvent('drop',file,document.createElement('footer')).defaultPrevented);assert.match(name.textContent,/Drop test.nes/);
 fileEvent('drop',{name:'Broken.nes',size:2,arrayBuffer:async()=>new Uint8Array(2).buffer});await flush();assert(send.disabled);assert.equal(connection.textContent,label);assert.equal(name.textContent,'No game selected');
 document.querySelector('#file').onchange({target:{files:[{...file,arrayBuffer:async()=>raw.buffer}]}});await flush();assert(!send.disabled);assert.equal(connection.textContent,label);
}
