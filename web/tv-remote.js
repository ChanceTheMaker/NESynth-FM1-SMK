// Fullscreen clicker: same channel and volume controls as the TV panel.
export function wireRemote({stepChannel,stepVolume,game}){
 const rig=document.querySelector('.retro-rig');
 const remote=document.createElement('aside');remote.id='tv-remote';remote.className='tv-remote';remote.hidden=true;remote.setAttribute('aria-label','TV remote control');
 remote.innerHTML=`<div class="remote-top"><button type="button" class="remote-grip" aria-label="Move remote: drag or use arrow keys" title="Drag to move">NESynth<small>REMOTE CONTROL</small></button><button type="button" class="remote-close" aria-label="Close remote" title="Close remote">×</button></div>
 <div class="remote-window" aria-hidden="true"><i></i></div>
 <div class="remote-display" aria-live="polite"><span>CH <b id="remote-channel">03</b></span><span>VOL <b id="remote-volume">25</b></span></div>
 <div class="remote-rockers"><div><span>CHANNEL</span><button type="button" data-remote="channel-up" aria-label="Channel up">▲</button><button type="button" data-remote="channel-down" aria-label="Channel down">▼</button></div><div><span>VOLUME</span><button type="button" data-remote="volume-up" aria-label="Volume up">＋</button><button type="button" data-remote="volume-down" aria-label="Volume down">−</button></div></div>
 <button type="button" class="remote-game" data-remote="game">GAME <span>CH 03</span></button><div class="remote-ridges" aria-hidden="true"></div>`;
 rig.append(remote);
 const launcher=document.createElement('button');launcher.id='show-remote';launcher.type='button';launcher.textContent='TV remote';launcher.hidden=true;launcher.setAttribute('aria-controls',remote.id);launcher.setAttribute('aria-expanded','false');
 rig.querySelector('.simulator-actions').prepend(launcher);
 const grip=remote.querySelector('.remote-grip');let wasOn=false,closed=false,moved=false,pointer=null,startX=0,startY=0,baseX=0,baseY=0;
 const fullscreen=()=>document.fullscreenElement===rig||rig.classList.contains('fullscreen-fallback');
 function position(x,y){
  const box=remote.getBoundingClientRect(),pad=8;
  remote.style.left=Math.max(pad,Math.min(innerWidth-box.width-pad,x))+'px';
  remote.style.top=Math.max(pad,Math.min(innerHeight-box.height-pad,y))+'px';
  remote.style.right=remote.style.bottom='auto';moved=true;
 }
 function fit(){if(moved&&!remote.hidden){const box=remote.getBoundingClientRect();position(box.x,box.y);}}
 function sync(){
  const on=fullscreen();if(on&&!wasOn)closed=false;wasOn=on;
  launcher.hidden=!on;remote.hidden=!on||closed;launcher.setAttribute('aria-expanded',String(!remote.hidden));fit();
 }
 remote.querySelector('.remote-close').onclick=()=>{closed=true;sync();launcher.focus({preventScroll:true});};
 launcher.onclick=()=>{closed=!closed;sync();if(!remote.hidden)remote.querySelector('[data-remote="channel-up"]').focus({preventScroll:true});};
 remote.addEventListener('click',e=>{
  const action=e.target.closest('[data-remote]')?.dataset.remote;
  if(action==='channel-up')stepChannel(1);else if(action==='channel-down')stepChannel(-1);
  else if(action==='volume-up')stepVolume(5);else if(action==='volume-down')stepVolume(-5);else if(action==='game')game();
 });
 // Keep controller shortcuts out of the clicker, while preserving native keys.
 for(const event of ['keydown','keyup'])remote.addEventListener(event,e=>{if(e.key!=='Escape')e.stopPropagation();});
 grip.addEventListener('pointerdown',e=>{
  if(e.button!==0)return;e.preventDefault();pointer=e.pointerId;startX=e.clientX;startY=e.clientY;
  const box=remote.getBoundingClientRect();baseX=box.x;baseY=box.y;grip.setPointerCapture(pointer);grip.focus({preventScroll:true});
 });
 grip.addEventListener('pointermove',e=>{if(e.pointerId===pointer)position(baseX+e.clientX-startX,baseY+e.clientY-startY);});
 for(const event of ['pointerup','pointercancel','lostpointercapture'])grip.addEventListener(event,e=>{if(e.pointerId===pointer)pointer=null;});
 grip.addEventListener('keydown',e=>{
  const direction={ArrowLeft:[-16,0],ArrowRight:[16,0],ArrowUp:[0,-16],ArrowDown:[0,16]}[e.key];
  if(direction){e.preventDefault();const box=remote.getBoundingClientRect();position(box.x+direction[0],box.y+direction[1]);}
 });
 document.addEventListener('fullscreenchange',sync);document.addEventListener('nesynth-fullscreen',sync);window.addEventListener('resize',fit);sync();
 return {channel(number,name){remote.querySelector('#remote-channel').textContent=number;remote.querySelector('#remote-channel').title=name;},volume(value){remote.querySelector('#remote-volume').textContent=value;}};
}
