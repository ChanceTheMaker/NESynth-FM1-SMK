import './simulator.js';
import './fullscreen.js?v=20261016-remote';
import './flash-ui.js?v=974';
import './main-loader.js?v=970';
import {openCartridge} from './zip.js';
import {paintFrame,BrowserAudio} from './media.js';
import {fetchHomebrew} from './homebrew.js';
import {normalizeCartridge} from './cartridge.js?v=20261016-headers';
import {wireTv} from './tv-demos.js?v=2';
const $=s=>document.querySelector(s), canvas=$('#screen'), ctx=canvas.getContext('2d',{alpha:false});
let archiveEntries=[],loading=false,installBusy=false,firmwareBusy=false,deviceBusy=false;
const errors=['','File is shorter than an iNES header.','Use a standard iNES 1.0 cartridge. NES 2.0 and special consoles are not supported.','Cartridge size is invalid for its mapper, or the file is truncated.','This cartridge requests unsupported RAM.','PAL cartridges are not supported yet.','This mapper is not supported yet. NESynth currently supports mappers 0 (NROM), 1 (MMC1), 2 (UxROM), 3 (CNROM), 4 (MMC3), 9 (MMC2) and 11 (Color Dreams).','Emulator error.','Emulator timing or audio queue limit reached.'];
let core,loaded=false,paused=false,saveKey='',name='',last=performance.now(),clock=0,frameBase=0,fpsTime=last;
let keys=new Set(),pointers=new Map(),externalControls=new Map();
const audio=new BrowserAudio(running=>{
    $('#sound').textContent=running?'Sound enabled':'Enable sound';$('#sound').disabled=running;
    $('#sound-state').textContent=running?'Audio on':'Audio paused — press Enable sound or a game control.';
});
let inputEpoch=0;const keySince=new Map(),pointerSince=new Map();
const keyBits=new Map([['ArrowUp',64],['KeyW',64],['ArrowDown',128],['KeyS',128],['ArrowLeft',32],['KeyA',32],['ArrowRight',16],['KeyD',16],['Enter',1],['KeyX',1],['Space',2],['KeyZ',2],['KeyP',8],['ShiftLeft',4],['ShiftRight',4]]);
const pixels=ctx.createImageData(256,240);
const tv=wireTv(canvas,()=>{stopAudio();release();clock=0;last=fpsTime=performance.now();frameBase=core?.gp_frame_count()||0;});
function status(text,error=false){$('#status').textContent=text;$('#status').classList.toggle('error',error);}
function updateButtons(){let bits=0;for(const k of keys)bits|=keyBits.get(k)||0;for(const bit of pointers.values())bits|=bit;for(const bit of externalControls.values())bits|=bit;core?.gp_buttons(bits);document.querySelectorAll('[data-bit]').forEach(el=>el.classList.toggle('held',!!(bits&Number(el.dataset.bit))));}
function release(){inputEpoch++;keys.clear();pointers.clear();externalControls.clear();keySince.clear();pointerSince.clear();updateButtons();}
function focusGame(){canvas.focus({preventScroll:true});}
function stopAudio(){audio.flush();}
function save(){if(!loaded||!saveKey||!core.gp_save_size()||!core.gp_save_dirty())return;try{const data=new Uint8Array(core.memory.buffer,core.gp_save_data(),core.gp_save_size());localStorage.setItem(saveKey,btoa(String.fromCharCode(...data)));core.gp_save_clean();}catch{status('Save storage unavailable. Use Export save to keep your progress.',true);}}
async function load(bytes,title){
    tv.reset();
    const normalized=await normalizeCartridge(bytes);bytes=normalized.bytes;
    document.querySelector('#game-guide')?.remove();
    document.querySelector('#cartridge-repair')?.remove();
    $('#diagnostic-hint').hidden=title!=='Original diagnostic';
    document.dispatchEvent(new Event('nesynth-rom-clear'));
    save();stopAudio();release();loaded=false;paused=false;$('#power').classList.remove('on');
    for(const id of ['pause','reset','export'])$('#'+id).disabled=true;$('#save').disabled=true;
    if(bytes.length>1024*1024)throw new Error('Cartridge exceeds the 1 MiB upload limit.');
    new Uint8Array(core.memory.buffer,core.gp_upload(),bytes.length).set(bytes);
    const code=core.gp_load_upload(bytes.length);if(code)throw new Error((code===6?'Mapper '+((bytes[6]>>4)|(bytes[7]&240))+': ':'')+(errors[code]||'Cartridge rejected.'));
    const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),x=>x.toString(16).padStart(2,'0')).join('');
    saveKey='nes-fm1-sav:'+hash;name=title;
    try{const cached=localStorage.getItem(saveKey);if(cached&&core.gp_save_size()){const data=Uint8Array.from(atob(cached),x=>x.charCodeAt(0));if(data.length===core.gp_save_size())new Uint8Array(core.memory.buffer,core.gp_save_data(),data.length).set(data);}}catch{}
    core.gp_save_clean();loaded=true;clock=0;last=fpsTime=performance.now();frameBase=0;
    $('#pause').disabled=$('#reset').disabled=false;$('#pause').textContent='Pause';$('#power').classList.add('on');$('#title').textContent=title;
    document.dispatchEvent(new CustomEvent('nesynth-rom',{detail:{bytes,name:title,title}}));
    const mapper=(bytes[6]>>4)|(bytes[7]&240);
    $('#export').disabled=$('#save').disabled=!core.gp_save_size();status('Running locally · NTSC / mapper '+mapper+' ('+(({0:'NROM',1:'MMC1',2:'UxROM',3:'CNROM',4:'MMC3',9:'MMC2',11:'Color Dreams'})[mapper])+')');
    if(normalized.notice){const note=document.createElement('p');note.id='cartridge-repair';note.className='hint';note.textContent=normalized.notice;$('#status').after(note);}
}
async function sound(){
    try{await audio.enable();}
    catch{$('#sound').disabled=false;$('#sound-state').textContent='Audio could not start. Press Enable sound to retry.';}
}
function frame(){
    if(!core.gp_frame()){if(core.gp_error()){paused=true;stopAudio();status(errors[core.gp_error()]||'Emulator stopped.',true);}return false;}
    const count=core.gp_audio_queued(),ptr=core.gp_render_audio(count),pcm=new Int16Array(core.memory.buffer,ptr,count*2);
    audio.queue(pcm,count);
    return true;
}
setInterval(()=>{
    const now=performance.now(),elapsed=Math.min(100,now-last);last=now;
    if(!loaded||paused||tv.active||document.hidden)return;
    if(audio.running){
        for(let i=0;i<6&&audio.time<audio.context.currentTime+.08;i++)if(!frame())break;
        clock=0;
    }else{clock+=elapsed;const step=1000*59561/3579545;for(let i=0;i<6&&clock>=step;i++){clock-=step;if(!frame())break;}}
},8);
function draw(now){
    if(tv.draw(now)){$('#fps').textContent='DEMO TV';}
    else if(loaded){const src=new Uint8Array(core.memory.buffer,core.gp_pixels(),256*240),pal=new Uint32Array(core.memory.buffer,core.np_palette(),64);paintFrame(src,pal,pixels.data);ctx.putImageData(pixels,0,0);
        if(now-fpsTime>=1000){const n=core.gp_frame_count();$('#fps').textContent=paused?'PAUSED':`${((n-frameBase)*1000/(now-fpsTime)).toFixed(1)} FPS`;frameBase=n;fpsTime=now;}}
    requestAnimationFrame(draw);
}requestAnimationFrame(draw);
async function fileAction(action){
    if(loading||installBusy)return;loading=true;document.dispatchEvent(new CustomEvent('nesynth-loading',{detail:true}));
    document.dispatchEvent(new Event('nesynth-rom-clear'));
    syncHomebrewButtons();
    for(const id of ['diagnostic','rom','archive-play','archive-entry','folder','library-play'])$('#'+id).disabled=true;
    try{await action();}catch(e){status(e.message,true);document.dispatchEvent(new CustomEvent('nesynth-load-error',{detail:e.message}));}
    finally{loading=false;syncHomebrewButtons();document.dispatchEvent(new CustomEvent('nesynth-loading',{detail:false}));for(const id of ['diagnostic','rom','archive-play','archive-entry','folder','library-play'])$('#'+id).disabled=installBusy;}
}
function syncHomebrewButtons(){document.querySelectorAll('[data-homebrew],#dropzone').forEach(button=>{button.disabled=!core||loading||installBusy;});}
$('#dropzone').onclick=()=>{if(!core||loading||installBusy)return;$('#rom').click();};
document.querySelectorAll('[data-homebrew]').forEach(button=>button.addEventListener('click',()=>{
    if(!core||loading||installBusy)return;
    const preview= !button.hasAttribute('data-install');if(preview)void sound();
    void fileAction(async()=>{
        status('Opening homebrew cartridge…');
        const {game,bytes}=await fetchHomebrew(button.dataset.homebrew);
        clearArchive();await load(bytes,game.title);
        const guide=document.createElement('p');guide.id='game-guide';guide.className='hint game-guide';guide.textContent=game.controls;
        $('#status').after(guide);if(preview){$('#browser-player').scrollIntoView({block:'start'});focusGame();}
    });
}));
function clearArchive(){archiveEntries=[];$('#archive-picker').hidden=true;$('#archive-entry').replaceChildren();}
async function playEntry(entry){status('Opening '+entry.name+'…');await load(await entry.read(),entry.name.replaceAll('\\','/').split('/').pop());}
$('#diagnostic').onclick=()=>{void sound();return fileAction(async()=>{clearArchive();await load(new Uint8Array(await(await fetch('diagnostic.nes')).arrayBuffer()),'Original diagnostic');focusGame();});};
async function loadCartridgeFiles(files){
    if(!files.length)return;
    if(!core||$('#rom').disabled){status('Please wait until the cartridge loader is ready.',true);return;}
    if(files.length!==1){status('Choose one .nes or .zip file at a time.',true);return;}
    const f=files[0];
    if(!/\.(nes|zip)$/i.test(f.name)){status('Choose a .nes cartridge or a .zip archive.',true);return;}
    void sound();
    await fileAction(async()=>{
        clearArchive();status('Opening '+f.name+'…');archiveEntries=await openCartridge(f);
        if(archiveEntries.length===1){await playEntry(archiveEntries[0]);archiveEntries=[];}
        else{
            archiveEntries.forEach((entry,i)=>$('#archive-entry').add(new Option(entry.name,String(i))));
            $('#archive-picker').hidden=false;$('#archive-name').textContent=f.name;
            status(archiveEntries.length+' cartridges found. Choose one, then press Load cartridge.');
        }
    });
    if(!$('#archive-picker').hidden){$('#archive-picker').scrollIntoView({block:'center'});$('#archive-entry').focus();}
}
let localLibrary=[];
function showLibrary(){
 const query=$('#library-search').value.toLowerCase(),matches=localLibrary.filter(f=>(f.webkitRelativePath||f.name).toLowerCase().includes(query));
 $('#library').replaceChildren(...matches.map(f=>new Option(f.webkitRelativePath||f.name,String(localLibrary.indexOf(f)))));
 $('#library-count').textContent=matches.length+' of '+localLibrary.length+' NES / ZIP files';
}
$('#folder').onchange=e=>{localLibrary=Array.from(e.target.files).filter(f=>/\.(nes|zip)$/i.test(f.name)).sort((a,b)=>(a.webkitRelativePath||a.name).localeCompare(b.webkitRelativePath||b.name));$('#library-search').value='';showLibrary();e.target.value='';};
$('#library-search').oninput=showLibrary;
$('#library-play').onclick=()=>{const f=localLibrary[Number($('#library').value)];if($('#library').value&&f)void loadCartridgeFiles([f]);};
$('#rom').onchange=async e=>{const files=Array.from(e.target.files);e.target.value='';await loadCartridgeFiles(files);};
const dropTargets=[$('#dropzone'),$('.retro-rig')],dragDepth=new Map();
const hasFiles=e=>Array.from(e.dataTransfer?.types||[]).includes('Files');
function clearDrop(){dragDepth.clear();for(const target of dropTargets)target.classList.remove('drag-over');}
for(const target of dropTargets){
    target.addEventListener('dragenter',e=>{
        if(!hasFiles(e))return;e.preventDefault();e.stopPropagation();
        dragDepth.set(target,(dragDepth.get(target)||0)+1);
        if(!$('#rom').disabled)target.classList.add('drag-over');
    });
    target.addEventListener('dragover',e=>{
        if(!hasFiles(e))return;e.preventDefault();e.stopPropagation();
        e.dataTransfer.dropEffect=$('#rom').disabled?'none':'copy';
    });
    target.addEventListener('dragleave',()=>{
        const depth=Math.max(0,(dragDepth.get(target)||0)-1);dragDepth.set(target,depth);
        if(!depth)target.classList.remove('drag-over');
    });
    target.addEventListener('drop',e=>{
        if(!hasFiles(e))return;e.preventDefault();e.stopPropagation();clearDrop();
        void loadCartridgeFiles(Array.from(e.dataTransfer.files));
    });
}
// File drops elsewhere must not navigate away from the running cartridge.
document.addEventListener('dragover',e=>{if(hasFiles(e)){e.preventDefault();e.dataTransfer.dropEffect='none';}});
document.addEventListener('drop',e=>{
    if(!hasFiles(e))return;e.preventDefault();clearDrop();
    status('Drop your .nes or .zip file onto the screen, console, or cartridge area.',true);
});
document.addEventListener('dragend',clearDrop);window.addEventListener('blur',clearDrop);
$('#archive-play').onclick=()=>{void sound();return fileAction(async()=>{const entry=archiveEntries[Number($('#archive-entry').value)];if(entry)await playEntry(entry);});};
$('#sound').onclick=()=>{void sound();focusGame();};
$('#volume').oninput=e=>{core?.gp_volume(Number(e.target.value));$('#level').textContent=e.target.value+'%';};
$('#pause').onclick=()=>{paused=!paused;core.gp_pause(paused?1:0);stopAudio();release();clock=0;$('#pause').textContent=paused?'Resume':'Pause';if(!paused){void sound();focusGame();}};
$('#reset').onclick=()=>{void sound();stopAudio();release();core.gp_reset();paused=false;clock=0;frameBase=0;fpsTime=performance.now();$('#pause').textContent='Pause';focusGame();};
document.addEventListener('nesynth-control',e=>{const {source,bit,down}=e.detail;if(down){void sound();externalControls.set(source,bit);}else externalControls.delete(source);updateButtons();});
document.addEventListener('nesynth-release',release);
function syncDeviceBusy(){installBusy=firmwareBusy||deviceBusy;syncHomebrewButtons();for(const id of ['diagnostic','rom','archive-play','archive-entry','folder','library-play'])$('#'+id).disabled=installBusy||loading||!core;$('#pause').disabled=$('#reset').disabled=installBusy||!loaded;}
document.addEventListener('nesynth-install-busy',e=>{firmwareBusy=e.detail;syncDeviceBusy();});
document.addEventListener('nesynth-device-state',e=>{deviceBusy=e.detail.busy;syncDeviceBusy();});
document.addEventListener('nesynth-pause',()=>{if(!loaded)return;paused=true;core.gp_pause(1);stopAudio();release();save();clock=0;$('#pause').textContent='Resume';});
window.addEventListener('keydown',e=>{if(e.target instanceof HTMLInputElement||e.target instanceof HTMLButtonElement||e.target instanceof HTMLSelectElement)return;if(keyBits.has(e.code)){e.preventDefault();void sound();if(!e.repeat)keySince.set(e.code,performance.now());keys.add(e.code);updateButtons();}});
window.addEventListener('keyup',e=>{if(keyBits.has(e.code)){e.preventDefault();const epoch=inputEpoch,stamp=keySince.get(e.code);setTimeout(()=>{if(epoch===inputEpoch&&keySince.get(e.code)===stamp){keys.delete(e.code);updateButtons();}},Math.max(0,40-(performance.now()-stamp)));}});
window.addEventListener('blur',release);document.addEventListener('visibilitychange',()=>{release();stopAudio();save();last=performance.now();clock=0;});
document.querySelectorAll('[data-bit]').forEach(el=>{el.onpointerdown=e=>{e.preventDefault();void sound();el.setPointerCapture(e.pointerId);pointerSince.set(e.pointerId,performance.now());pointers.set(e.pointerId,Number(el.dataset.bit));updateButtons();};const up=e=>{const epoch=inputEpoch,stamp=pointerSince.get(e.pointerId);setTimeout(()=>{if(epoch===inputEpoch&&pointerSince.get(e.pointerId)===stamp){pointers.delete(e.pointerId);updateButtons();}},Math.max(0,40-(performance.now()-stamp)));};el.onpointerup=el.onpointercancel=el.onlostpointercapture=up;});
$('#export').onclick=()=>{const data=new Uint8Array(core.memory.buffer,core.gp_save_data(),core.gp_save_size()).slice(),url=URL.createObjectURL(new Blob([data]));const a=document.createElement('a');a.href=url;a.download=name.replace(/\.nes$/i,'')+'.sav';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
$('#save').onchange=async e=>{const f=e.target.files[0];if(!f)return;const data=new Uint8Array(await f.arrayBuffer());if(!loaded||data.length!==core.gp_save_size()){status('Save size does not match this cartridge.',true);return;}new Uint8Array(core.memory.buffer,core.gp_save_data(),data.length).set(data);save();status('Battery RAM imported. Reset the cartridge if needed.');e.target.value='';};
setInterval(save,2000);window.addEventListener('pagehide',save);
try{const res=await fetch('nes.wasm?v=969');if(!res.ok)throw new Error('Build the WebAssembly core first.');core=(await WebAssembly.instantiate(await res.arrayBuffer(),{})).instance.exports;for(const id of ['diagnostic','rom','sound'])$('#'+id).disabled=false;await load(new Uint8Array(await(await fetch('diagnostic.nes')).arrayBuffer()),'Original diagnostic');syncHomebrewButtons();}catch(e){status(e.message,true);}
