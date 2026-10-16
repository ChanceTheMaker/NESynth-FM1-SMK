// Original procedural demoscene effects. No cartridge data or device access.
import {wireRemote} from './tv-remote.js?v=1';
const names=['STARFIELD','PLASMA','TEXTURE TUNNEL','COPPER BARS','ROTOZOOM','FIRE','METABALLS','DOT SPHERE','WIRE CUBE','SYNTHWAVE','SINE SCROLLER','BOING BALL'];
const wrap=n=>((n%13)+13)%13;
export function wireTv(canvas,onChannelChange){
 const channel=document.querySelector('.tv-channel'),digits=channel.querySelector('b');
 const tuning=document.querySelector('.tv-dial:not(.small)'),volume=document.querySelector('.tv-dial.small');
 const slider=document.getElementById('volume'),ctx=canvas.getContext('2d');
 const buffer=document.createElement('canvas');buffer.width=128;buffer.height=120;
 const pixelCtx=buffer.getContext('2d'),pixels=pixelCtx.createImageData(128,120);
 let selected=0,started=performance.now(),last=-Infinity,remote;
 const api={get active(){return selected!==0;},reset:()=>select(0),draw};
 function select(next){
  next=wrap(next);if(next===selected)return;
  selected=next;started=performance.now();last=-Infinity;sync();onChannelChange(api.active);
 }
 function sync(){
  const number=String(selected+3).padStart(2,'0');digits.textContent=number;
  channel.setAttribute('aria-valuenow',String(selected+3));
  channel.setAttribute('aria-valuetext',selected?number+' '+names[selected-1]:'03 Game');
  tuning.style.setProperty('--dial-turn',(selected*27+30)+'deg');
  canvas.dataset.tvChannel=number;
  remote?.channel(number,selected?names[selected-1]:'Game');
 }
 function drag(el,get,set,step,clickStep=0){
  let pointer=null,x=0,y=0,value=0,moved=false;
  el.addEventListener('pointerdown',e=>{
   if(e.button!==0)return;e.preventDefault();e.stopPropagation();
   pointer=e.pointerId;x=e.clientX;y=e.clientY;value=get();moved=false;el.setPointerCapture(pointer);
   if(el.tabIndex>=0)el.focus({preventScroll:true});
  });
  el.addEventListener('pointermove',e=>{
   if(e.pointerId!==pointer)return;
   if(Math.abs(e.clientX-x)+Math.abs(e.clientY-y)>4)moved=true;
   const change=Math.trunc(((e.clientX-x)-(e.clientY-y))/step);set(value+change);
  });
  const finish=e=>{
   if(e.pointerId!==pointer)return;
   if(e.type==='pointerup'&&!moved&&clickStep){const box=el.getBoundingClientRect();set(get()+(e.clientX<box.x+box.width/2?-clickStep:clickStep));}
   pointer=null;
  };
  for(const event of ['pointerup','pointercancel','lostpointercapture'])el.addEventListener(event,finish);
 }
 channel.setAttribute('role','slider');channel.tabIndex=0;
 channel.setAttribute('aria-label','TV channel');channel.setAttribute('aria-valuemin','3');channel.setAttribute('aria-valuemax','15');
 channel.title='Drag up or right to tune. Channel 03 returns to your game.';
 tuning.title='Click left for channel down, right for channel up. Or drag to tune.';
 for(const el of [channel,tuning])drag(el,()=>selected,select,22,el===tuning?1:0);
 channel.addEventListener('keydown',e=>{
  if(!['ArrowUp','ArrowRight','ArrowDown','ArrowLeft','Home','End'].includes(e.key))return;
  e.preventDefault();e.stopPropagation();select(e.key==='Home'?0:e.key==='End'?12:selected+(['ArrowUp','ArrowRight'].includes(e.key)?1:-1));
 });
 function setVolume(value){slider.value=String(Math.max(0,Math.min(100,value)));slider.dispatchEvent(new Event('input',{bubbles:true}));}
 function syncVolume(){const value=Number(slider.value);volume.setAttribute('aria-valuenow',String(value));volume.setAttribute('aria-valuetext',value+' percent');volume.style.setProperty('--dial-turn',(-130+value*2.6)+'deg');remote?.volume(value);}
 volume.setAttribute('role','slider');volume.tabIndex=0;volume.setAttribute('aria-label','TV volume');
 volume.setAttribute('aria-valuemin','0');volume.setAttribute('aria-valuemax','100');volume.title='Click left for volume down, right for volume up. Or drag. Synced with the browser slider.';
 drag(volume,()=>Number(slider.value),setVolume,2,5);
 volume.addEventListener('keydown',e=>{
  if(!['ArrowUp','ArrowRight','ArrowDown','ArrowLeft','Home','End'].includes(e.key))return;
  e.preventDefault();e.stopPropagation();setVolume(e.key==='Home'?0:e.key==='End'?100:Number(slider.value)+(['ArrowUp','ArrowRight'].includes(e.key)?5:-5));
 });
 remote=wireRemote({stepChannel:delta=>select(selected+delta),stepVolume:delta=>setVolume(Number(slider.value)+delta),game:()=>select(0)});
 slider.addEventListener('input',syncVolume);syncVolume();sync();
 document.getElementById('simulator-model').addEventListener('change',()=>api.reset());
 document.addEventListener('keydown',e=>{if(e.key==='Escape'&&api.active)api.reset();});
 const color=(h,l=55)=>`hsl(${h} 95% ${l}%)`;
 function line(x,y,a,b,c,width=1){ctx.strokeStyle=c;ctx.lineWidth=width;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(a,b);ctx.stroke();}
 function raster(effect,t){
  const data=pixels.data;
  for(let y=0;y<120;y++)for(let x=0;x<128;x++){
   const dx=x-64,dy=y-60,r=Math.hypot(dx,dy)+.1,a=Math.atan2(dy,dx);let v=0,R=0,G=0,B=0;
   if(effect===1){
    v=Math.sin(x*.09+t)+Math.sin(y*.12-t*.7)+Math.sin((x+y)*.06+t*.6)+Math.sin(r*.14-t);
    R=128+127*Math.sin(v+t*.3);G=128+127*Math.sin(v+2.1);B=128+127*Math.sin(v+4.2);
   }else if(effect===2){
    const u=(a/Math.PI*32+t*7)|0,w=(450/r+t*18)|0;v=((u^w)&15)*16;
    const shade=Math.min(1,r/30);R=v*.45*shade;G=(70+v*.7)*shade;B=(150+v*.4)*shade;
   }else if(effect===4){
    const scale=1.4+Math.sin(t*.6)*.8,u=(dx*Math.cos(t*.3)-dy*Math.sin(t*.3))*scale+t*12,w=(dx*Math.sin(t*.3)+dy*Math.cos(t*.3))*scale;
    v=(((u>>3)^(w>>3))&1);R=v?230:28;G=v?71:25;B=v?164:69;
   }else if(effect===5){
    const heat=(y/120)*1.4-.25+.2*Math.sin(x*.14+Math.sin(y*.05-t*1.5)*2)+.1*Math.sin(x*.4+y*.12-t*2);
    R=Math.min(255,heat*600);G=Math.max(0,(heat-.35)*430);B=Math.max(0,(heat-.8)*700);
   }else{
    for(let k=0;k<5;k++){const px=64+Math.sin(t*.6+k*2)*43,py=60+Math.cos(t*.5+k*1.7)*40;v+=170/((x-px)**2+(y-py)**2+1);}
    const edge=Math.max(0,1-Math.abs(v-1.5)*2);
    R=v>1.5?35+edge*160:8;G=v>1.5?155+edge*100:12;B=v>1.5?180+edge*75:32;
   }
   const i=(y*128+x)*4;data[i]=R;data[i+1]=G;data[i+2]=B;data[i+3]=255;
  }
  pixelCtx.putImageData(pixels,0,0);ctx.imageSmoothingEnabled=false;ctx.drawImage(buffer,0,0,256,240);
 }
 function project(x,y,z,t){
  const a=x*Math.cos(t)-z*Math.sin(t),b=x*Math.sin(t)+z*Math.cos(t);
  const c=y*Math.cos(t*.7)-b*Math.sin(t*.7),d=y*Math.sin(t*.7)+b*Math.cos(t*.7);
  return [128+a*170/(d+4),116+c*170/(d+4),d];
 }
 function draw(now){
  if(!selected)return false;if(now-last<1000/30)return true;last=now;
  const t=(now-started)/1000,effect=selected-1;
  ctx.fillStyle='#070817';ctx.fillRect(0,0,256,240);
  if([1,2,4,5,6].includes(effect))raster(effect,t);
  else if(effect===0){
   for(let i=0;i<180;i++){
    const z=1-((i*.618033+t*.16)%1),x=((i*97)%251-125)/z*.35,y=((i*53)%239-119)/z*.35;
    const tail=1+.025/z;line(128+x,120+y,128+x*tail,120+y*tail,color(190+i%90,40+(1-z)*55),Math.max(1,(1-z)*2));
   }
  }else if(effect===3){
   for(let k=0;k<9;k++){
    const y=115+Math.sin(t*.8+k*.47)*70;
    for(let j=-9;j<=9;j++){ctx.fillStyle=color(k*37+t*10,20+(1-Math.abs(j)/10)*55);ctx.fillRect(0,y+j,256,1);}
   }
   ctx.font='bold 34px monospace';ctx.textAlign='center';ctx.fillStyle='#e8f4ff';ctx.fillText('NESYNTH',128,128);
  }else if(effect===7){
   const dots=[];for(let j=1;j<17;j++)for(let k=0;k<32;k++){
    const a=j*Math.PI/17,b=k*Math.PI/16;dots.push(project(Math.sin(a)*Math.cos(b)*1.6,Math.cos(a)*1.6,Math.sin(a)*Math.sin(b)*1.6,t*.4));
   }
   dots.sort((a,b)=>b[2]-a[2]);for(const [x,y,z] of dots){ctx.fillStyle=color(180+z*40,55-z*15);ctx.fillRect(x,y,z<0?3:1,z<0?3:1);}
  }else if(effect===8){
   for(let size=1;size<=2;size++){
    const points=Array.from({length:8},(_,i)=>project((i&1?1:-1)*size*.65,(i&2?1:-1)*size*.65,(i&4?1:-1)*size*.65,t*.45));
    for(let i=0;i<8;i++)for(const bit of [1,2,4])if(!(i&bit)){const a=points[i],b=points[i|bit];line(a[0],a[1],b[0],b[1],color(size===1?320:180),2);}
   }
  }else if(effect===9){
   const sky=ctx.createLinearGradient(0,0,0,155);sky.addColorStop(0,'#120627');sky.addColorStop(1,'#a22070');ctx.fillStyle=sky;ctx.fillRect(0,0,256,155);
   ctx.save();ctx.beginPath();ctx.arc(128,89,44,0,Math.PI*2);ctx.clip();
   for(let y=45;y<134;y++){ctx.fillStyle=color(45-(y-45)*.5);if(y<95||y%9<5)ctx.fillRect(80,y,96,1);}ctx.restore();
   for(let i=-9;i<=9;i++)line(128+i*7,155,128+i*48,240,'#cc28c7');
   for(let j=0;j<14;j++){const p=((j/14+t*.12)%1)**2;line(0,155+p*85,256,155+p*85,'#cc28c7');}
   line(0,155,256,155,'#75e5fa',2);
  }else if(effect===10){
   for(let j=0;j<6;j++){
    ctx.beginPath();for(let x=0;x<256;x++){const y=120+Math.sin(x*.025+t+j*.3)*50;ctx.lineTo(x,y);}ctx.strokeStyle=color(j*45+t*10);ctx.lineWidth=2;ctx.stroke();
   }
   const message='   NESYNTH // GREETINGS TO THE SCENE // CODE + PIXELS + MUSIC // KEEP THE OLD MACHINES ALIVE!   ';
   ctx.font='bold 22px monospace';ctx.textAlign='left';
   const offset=t*48;for(let i=0;i<24;i++){const n=Math.floor(offset/14)+i,x=i*14-offset%14,y=126+Math.sin(x*.035+t)*25;ctx.fillStyle=color(i*12+t*20,75);ctx.fillText(message[n%message.length],x,y);}
  }else if(effect===11){
   for(let y=144;y<240;y+=12)for(let x=0;x<256;x+=16){ctx.fillStyle=((x/16+y/12)&1)?'#343147':'#19182b';ctx.fillRect(x,y,16,12);}
   const x=128+Math.sin(t*.8)*63,y=145-Math.abs(Math.sin(t*1.3))*87;
   ctx.fillStyle='#0008';ctx.beginPath();ctx.ellipse(x,191,37-(145-y)*.1,8,0,0,Math.PI*2);ctx.fill();
   for(let py=-35;py<=35;py++)for(let px=-35;px<=35;px++){
    if(px*px+py*py>1225)continue;const z=Math.sqrt(1225-px*px-py*py),u=Math.atan2(px,z)+t,v=Math.asin(py/35);
    const check=((Math.floor(u*4)^Math.floor(v*4))&1),shade=.45+.55*z/35;
    ctx.fillStyle=check?`rgb(${240*shade} ${45*shade} ${60*shade})`:`rgb(${245*shade} ${240*shade} ${232*shade})`;ctx.fillRect(x+px,y+py,1,1);
   }
  }
  ctx.textAlign='left';ctx.font='bold 10px monospace';ctx.fillStyle='#0009';ctx.fillRect(8,9,240,19);
  ctx.fillStyle='#b7ffb1';ctx.fillText('CH '+String(selected+3).padStart(2,'0')+' / '+names[effect],14,22);
  ctx.fillStyle='#0009';ctx.fillRect(8,218,240,15);ctx.font='8px monospace';ctx.fillStyle='#d2d5dc';ctx.fillText('NESYNTH DEMO TV     CH 03 / ESC: GAME',14,229);
  return true;
 }
 return api;
}
