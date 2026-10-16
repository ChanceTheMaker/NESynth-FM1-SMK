const $=id=>document.getElementById(id),ns='http://www.w3.org/2000/svg';
const names={1:'A',2:'B',4:'Select',8:'Start',16:'Right',32:'Left',64:'Up',128:'Down'};
const symbols={1:'A',2:'B',4:'SEL',8:'START',16:'\u2192',32:'\u2190',64:'\u2191',128:'\u2193'};
export const simulatorMappings={
 fm1:{base:5,count:27,keys:{0:32,2:32,4:128,6:16,7:16,3:64,5:64,24:2,26:1,11:4,12:8},pads:{}},
 smk37:{base:0,count:37,keys:{5:32,7:32,9:128,11:16,12:16,8:64,10:64,35:2,36:1},pads:{1:64,8:32,9:128,10:16,14:2,15:1}}
};
let generation=0,zoomed=false;const assets=new Map();
function send(source,bit,down){document.dispatchEvent(new CustomEvent('nesynth-control',{detail:{source,bit,down}}));}
function assign(svg,element,bit,label,{x,y,size=10}={}){
 if(!element||!bit)return;
 element.dataset.controlBit=bit;element.setAttribute('role','button');element.setAttribute('tabindex','0');element.setAttribute('aria-label',label+' - '+names[bit]);
 const color=bit<=2?'#ef493c':'#dbbba0';element.style.setProperty('--button-fill',color);if(element.tagName!=='use')element.style.fill=color;
 const title=document.createElementNS(ns,'title');title.textContent=label+' - '+names[bit];element.append(title);
 if(x!==undefined){const text=document.createElementNS(ns,'text');text.setAttribute('x',x);text.setAttribute('y',y);text.setAttribute('class','control-label');text.setAttribute('font-size',size);text.textContent=symbols[bit];svg.append(text);}
}
function wire(svg,model){
 const fm=model==='fm1',cfg=simulatorMappings[fm?'fm1':'smk37'];
 const indices={white:[],black:[]};for(let i=0;i<cfg.count;i++)indices[[1,3,6,8,10].includes((i+cfg.base)%12)?'black':'white'].push(i);
 if(fm){
 const keys=[...svg.querySelectorAll('use.product-control-ink')];
 for(const [color,offset] of [['white',0],['black',16]])indices[color].forEach((key,i)=>{const el=keys[offset+i];assign(svg,el,cfg.keys[key],color+' key '+(i+1),{x:Number(el.getAttribute('x'))+12,y:Number(el.getAttribute('y'))+32,size:9});});
 for(const [x,bit,label] of [[490,4,'PLAY'],[526,8,'RECORD']])assign(svg,svg.querySelector(`rect.product-button[x="${x}"][y="185"]`),bit,label,{x:x+14.5,y:222,size:7});
 }else{
 for(const color of ['white','black'])[...svg.querySelectorAll('.smk-'+color+'-key')].forEach((el,i)=>assign(svg,el,cfg.keys[indices[color][i]],color+' key '+(i+1),{x:Number(el.getAttribute('x'))+Number(el.getAttribute('width'))/2,y:color==='white'?363:309,size:13}));
 [...svg.querySelectorAll('.smk-pad')].forEach((el,i)=>{const col=Math.floor(i/2),bottom=i%2,bit=cfg.pads[col+(bottom?8:0)];assign(svg,el,bit,(bottom?'Bottom':'Top')+' pad '+(col+1),{x:Number(el.getAttribute('x'))+18.5,y:Number(el.getAttribute('y'))+19,size:12});});
 for(const [x,bit,label] of [[838,4,'PLAY'],[877,8,'RECORD']])assign(svg,svg.querySelector(`rect.smk-control[x="${x}"][y="197"]`),bit,label,{x:x+14.5,y:232,size:8});
 }
 // The brand stays behind the live display when its canvas is enlarged.
 const screen=document.createElementNS(ns,'foreignObject');const rect=fm?[200,95,103,93]:[137,80,72,65];
 ['x','y','width','height'].forEach((key,i)=>screen.setAttribute(key,rect[i]));screen.setAttribute('class','live-screen');
 const holder=document.createElement('div');holder.className='lcd-content';holder.append($('screen'));screen.append(holder);svg.append(screen);
}
export async function renderSimulator(next){
 if(!['fm1','smk-pro','smk-elite','nes'].includes(next))return;
 const current=++generation;document.dispatchEvent(new Event('nesynth-release'));
 try{
 let svg;
 if(next!=='nes'){
 if(!assets.has(next))assets.set(next,fetch('artwork/'+next+'.svg').then(r=>{if(!r.ok)throw Error('Device artwork unavailable.');return r.text();}).catch(e=>{assets.delete(next);throw e;}));
 svg=new DOMParser().parseFromString(await assets.get(next),'image/svg+xml').documentElement;
 if(current!==generation)return;
 svg.removeAttribute('role');svg.setAttribute('role','group');
 }
 const shell=document.querySelector('.screen-frame');shell.append($('screen'));
 document.querySelector('.console').dataset.simulator=next;$('simulator-model').value=next;
 document.querySelector('.screen-frame').hidden=next!=='nes';$('hardware').hidden=next==='nes';$('zoom-screen').hidden=next==='nes';$('large-screen').hidden=true;
 if(svg){wire(svg,next);$('instrument-art').replaceChildren(svg);$('hardware-legend').textContent=next==='fm1'?'Click the lit controls. White 1 or 2: left. White 3: down. White 4 or 5: right. Black 2 or 3: up. White 15 / 16: B / A. PLAY (left): Select. RECORD (right): Start.':'Click the lit controls. White 4 or 5: left. White 6: down. White 7 or 8: right. Black 4 or 5: up. White 21 / 22: B / A. Pads: bottom 1 / 2 / 3 move, top 2 up, bottom 7 / 8 B / A. PLAY (left): Select. RECORD (right): Start.';}

 applyZoom();
 }catch(e){$('hardware-legend').textContent=e.message;}
}
function applyZoom(){
 const hardware=!$('hardware').hidden;const large=hardware&&zoomed;
 $('large-screen').hidden=!large;$('zoom-screen').setAttribute('aria-pressed',String(zoomed));$('zoom-screen').textContent=zoomed?'Screen in device':'Enlarge screen';
 if(hardware){const lcd=$('instrument-art').querySelector('.lcd-content');(large?$('large-screen'):lcd).append($('screen'));lcd.parentElement.style.display=large?'none':'';}
}
if($('hardware')){
 $('zoom-screen').onclick=()=>{zoomed=!zoomed;applyZoom();};
 $('simulator-model').onchange=e=>renderSimulator(e.target.value);
 const host=$('hardware');
 host.addEventListener('pointerdown',e=>{const b=e.target.closest('[data-control-bit]');if(!b)return;e.preventDefault();b.setPointerCapture(e.pointerId);send('sim-'+e.pointerId,Number(b.dataset.controlBit),true);});
 for(const type of ['pointerup','pointercancel','lostpointercapture'])host.addEventListener(type,e=>send('sim-'+e.pointerId,0,false));
 host.addEventListener('click',e=>{const b=e.target.closest('[data-control-bit]');if(!b)return;$('screen').focus({preventScroll:true});const source=Symbol();send(source,Number(b.dataset.controlBit),true);setTimeout(()=>send(source,0,false),100);});
 host.addEventListener('keydown',e=>{const b=e.target.closest('[data-control-bit]');if(!b||!['Space','Enter'].includes(e.code))return;e.preventDefault();e.stopPropagation();send('sim-key-'+e.code,Number(b.dataset.controlBit),true);});
 host.addEventListener('keyup',e=>{if(!['Space','Enter'].includes(e.code))return;send('sim-key-'+e.code,0,false);});
 host.addEventListener('focusout',()=>document.dispatchEvent(new Event('nesynth-release')));
 renderSimulator('nes');
}
