// Browser presentation only: the emulator and hardware palette remain unchanged.
export function paintFrame(indices,palette,rgba){
 for(let i=0;i<indices.length;i++){
  const color=palette[indices[i]&63],o=i*4;
  rgba[o]=(color>>>16)&255;rgba[o+1]=(color>>>8)&255;rgba[o+2]=color&255;rgba[o+3]=255;
 }
}

export class BrowserAudio{
 constructor(onState=()=>{},Context=globalThis.AudioContext){
  this.Context=Context;this.onState=onState;this.context=null;this.sources=new Set();this.time=0;this.pending=null;
 }
 get running(){return this.context?.state==='running';}
 async enable(){
  if(this.running)return;
  if(this.pending)return this.pending;
  if(!this.Context)throw Error('Web Audio is unavailable in this browser.');
  if(!this.context||this.context.state==='closed'){
   this.context=new this.Context({sampleRate:44100});
   this.context.onstatechange=()=>{if(!this.running)this.flush();this.onState(this.running);};
  }
  // Call resume during the click/drop/key gesture, before any ROM reads await.
  this.pending=this.context.resume();
  try{await this.pending;this.onState(this.running);}
  finally{this.pending=null;}
 }
 flush(){for(const source of this.sources){try{source.stop();}catch{}}this.sources.clear();this.time=0;}
 queue(pcm,count){
  if(!this.running||!count)return;
  const ctx=this.context,buffer=ctx.createBuffer(1,count,44100),out=buffer.getChannelData(0);
  for(let i=0;i<count;i++)out[i]=pcm[i*2]/32768;
  const source=ctx.createBufferSource();source.buffer=buffer;source.connect(ctx.destination);
  source.onended=()=>{this.sources.delete(source);source.disconnect();};this.sources.add(source);
  this.time=Math.max(this.time,ctx.currentTime+.025);source.start(this.time);this.time+=count/44100;
 }
}
