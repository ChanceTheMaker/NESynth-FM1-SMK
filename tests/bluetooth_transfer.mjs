import assert from 'node:assert/strict';
import {transferCartridge,decodeLoaderStatus} from '../web/bluetooth-transfer.js';
import {crc32} from '../web/zip.js';
const rom=Uint8Array.from({length:40976},(_,i)=>(i*31+7)&255);
function device({drop=false,stall=false,small=false,badAck=false,badCrc=false}={}){
 let state=0,error=0,received=0,buffer,session,crc,reads=0,dataWrites=0,dropped=false,committed=false,commands=0;
 return {
  read:async()=>{reads++;return {state,error,received:badAck&&reads===2?rom.length+1:received,capacity:262672};},
  write:async(p,response)=>{
   const v=new DataView(p.buffer,p.byteOffset,p.byteLength),u=o=>v.getUint32(o,true);
   if(p[0]===4){assert(response);state=0;error=received=0;return;}
   if(p[0]===1){assert(response);session=u(1);buffer=new Uint8Array(u(5));crc=u(9);state=1;return;}
   if(p[0]===3){assert(response);assert.equal(u(1),session);assert.equal(received,buffer.length);assert.equal(crc32(buffer),crc);assert.deepEqual(buffer,rom);state=badCrc?3:2;error=badCrc?3:0;committed=!badCrc;return;}
   assert.equal(p[0],2);dataWrites++;
   if(small&&p.length>20)throw Error('ATT payload exceeds MTU');
   if(!response){commands++;if(stall||drop&&!dropped&&commands===3){dropped=true;return;}}
   const off=u(1),data=p.subarray(5);
   if(off>received){error=2;return;}
   if(off<received){assert.deepEqual(buffer.subarray(off,off+data.length),data);error=0;return;}
   buffer.set(data,off);received+=data.length;error=0;
  },
  stats:()=>({reads,dataWrites,commands,committed,received})
 };
}
for(const options of [{},{drop:true},{small:true}]){
 const d=device(options);await transferCartridge({rom,session:123,burst:true,...d});assert(d.stats().committed);
 if(!options.small)assert(d.stats().reads<40,'One cumulative ACK per burst, not per packet');
}
const legacy=device();await transferCartridge({rom,session:124,burst:false,...legacy});assert(legacy.stats().committed);assert.equal(legacy.stats().commands,0);
for(const options of [{stall:true},{badAck:true},{badCrc:true}]){
 const d=device(options);await assert.rejects(transferCartridge({rom,session:125,burst:true,...d}));assert(!d.stats().committed);
}
assert.throws(()=>decodeLoaderStatus(new Uint8Array(20)));
console.log('PASS: burst transfer, lost-packet replay, bounded stall, impossible ACK, CRC rejection, small MTU and legacy firmware.');
