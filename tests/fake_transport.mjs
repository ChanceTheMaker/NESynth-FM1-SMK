import assert from 'node:assert/strict';
// Simulate handoffs and failures without opening any MIDI ports.
export function transport({start='FM-1_930',restart='FM-1_930',loader=true,stage=true,write=true,send=true}={}){
 const calls=[],links=[];
 const device=text=>{if(!text)return null;const model=text.slice(0,text.lastIndexOf('_')),version=Number(text.slice(text.lastIndexOf('_')+1));
  const link={closed:false,send(){calls.push('send');return send;},async close(){this.closed=true;}};links.push(link);
  return {name:'Test MIDI',id:{text,model,version},link};};
 class Transport{
  async findAny(filter){calls.push('find');const d=device(start);if(d&&!filter(d.id)){await d.link.close();return null;}return d;}
  async waitForAny(filter){const id=filter({model:'FM-1'})?restart:loader?(start.startsWith('Doom-')?'ota-Doom-FM-1_007':'ota-FM-1_001'):null;calls.push('wait:'+id);return device(id);}
  async serve(link,image,base){assert.equal(base,0xe0000000);calls.push('stage');this.image=image;return {finished:stage};}
  async write(dev,image){calls.push('write');Transport.image=image.slice();return {finished:write};}
 }
 return {Transport,calls,links};
}
