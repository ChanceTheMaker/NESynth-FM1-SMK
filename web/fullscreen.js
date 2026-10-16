// Expand the existing simulator; preserve the one live canvas and controls.
const rig=document.querySelector('.retro-rig'),button=document.getElementById('fullscreen');
if(rig&&button){
 let pending=false;
 const active=()=>document.fullscreenElement===rig||rig.classList.contains('fullscreen-fallback');
 function sync(){const on=active();button.setAttribute('aria-pressed',String(on));button.textContent=on?'Exit full screen':'Full screen';}
 async function toggle(){
  if(pending)return;pending=true;
  try{
   if(document.fullscreenElement===rig)await document.exitFullscreen();
   else if(rig.classList.contains('fullscreen-fallback')){rig.classList.remove('fullscreen-fallback');document.body.classList.remove('simulator-fullscreen');}
   else if(rig.requestFullscreen)await rig.requestFullscreen();
   else{rig.classList.add('fullscreen-fallback');document.body.classList.add('simulator-fullscreen');}
  }catch{button.title='Full screen was blocked by your browser. Try again from this button.';}
  finally{pending=false;sync();}
 }
 button.addEventListener('click',toggle);
 document.addEventListener('fullscreenchange',sync);
 document.addEventListener('keydown',e=>{if(e.code==='Escape'&&rig.classList.contains('fullscreen-fallback')){e.preventDefault();toggle();}});
 sync();
}
