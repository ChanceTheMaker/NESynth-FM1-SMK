import {prepareFirmware} from './rom-firmware.js?v=960';
import {Updater} from './vendor/fm1ota.js?v=20261013';
import {installFirmware,fm1Identity} from './install-firmware.js?v=20261013';
import {restoreLatest} from './restore-firmware.js?v=20261013';
import {createProgress} from './update-progress.js?v=20261013';
const $=id=>document.getElementById(id);
let rom=null,filename='',prepared=null,access=null,busy=false,downloadUrl=null,lastAction=null,loading=false;
let connectionChecked=false,installed=false,operation='';
const progress=createProgress($('install-log-heading'));
const message=(s,error=false)=>{$('flash-status').textContent=s;$('flash-status').classList.toggle('error',error);};
async function showAvailableVersion(){
 try{
  const response=await fetch('firmware/template.json',{cache:'no-store'});
  if(!response.ok)throw Error('Template unavailable');
  const metadata=await response.json();
  if(!/^FM-1_\d+$/.test(metadata.product))throw Error('Unknown firmware version');
  if(!prepared)$('firmware-version').textContent='Firmware: '+metadata.product;
 }catch{if(!prepared)$('firmware-version').textContent='Firmware version shown after preparation.';}
}

function controls(){
 const fm=$('flash-model').value==='fm1';
 $('fm1-steps').hidden=!fm;
 $('firmware-version').hidden=!fm;
 $('build-review').hidden=!fm||!prepared;
 $('build-firmware').disabled=busy||loading||!rom||!fm;$('connect-device').disabled=busy||loading||!fm;
 $('flash-device').disabled=busy||loading||!prepared||!fm;$('flash-model').disabled=busy||loading;
 $('restore-device').disabled=busy||loading||!fm;$('retry-install').disabled=busy||loading;
 $('build-firmware').textContent=operation==='prepare'?'Preparing game…':prepared?'Prepare game again':'Prepare game for FM-1';
 $('connect-device').textContent=operation==='connect'?'Connecting…':connectionChecked?'Check connection again':'Connect FM-1';
 $('flash-device').textContent=operation==='game'?'Installing…':installed?'Install game again':'Install game on FM-1';
 $('game-state').textContent=loading?'Loading…':rom?'Selected':'Choose a game';
 $('connect-state').textContent=connectionChecked?'Checked':'USB';
 $('prepare-state').textContent=prepared?'Ready':'Not prepared';
 $('install-state').textContent=installed?'Verified':'Not installed';
 const completed=[!!rom,connectionChecked,!!prepared,installed];
 ['game','connect','prepare','install'].forEach((step,i)=>{
  $('step-'+step).dataset.complete=String(completed[i]);
  $('step-'+step).dataset.current=String(i===completed.indexOf(false));
 });
 $('install-help').textContent=installed?'Restart verified. Your game is running on the FM-1.':!rom?'Choose a game in step 1 to get started.':!prepared?'Press Prepare game for FM-1 in step 3 to enable installation.':connectionChecked?'Ready. Press Install game on FM-1 to transfer your game.':'Plug in your FM-1. Install will check the connection and request MIDI permission.';
 document.dispatchEvent(new CustomEvent('nesynth-install-busy',{detail:busy}));
}
function available(){
 $('flash-availability').textContent=$('flash-model').value==='fm1'?
  'USB installation available · Chrome or Edge on a computer.':
  'SMK37 installation is not available yet. Use the browser player’s “Play on” selector to try its controls, or choose FM-1 here to install.';
 controls();
}
function log(text){
 const lines=$('install-log').textContent.split('\n').filter(Boolean);
 lines.push(new Date().toLocaleTimeString()+'  '+text);
 $('install-log').textContent=lines.slice(-200).join('\n');
 $('install-log').scrollTop=$('install-log').scrollHeight;
}
function reveal(){
 $('install-progress').open=true;$('install-progress').scrollIntoView({block:'start',behavior:'smooth'});
 $('install-progress').focus({preventScroll:true});
}
async function midi(){
 if(!window.isSecureContext||!navigator.requestMIDIAccess)throw Error('Open this site in Chrome or Edge over HTTPS (or localhost) for USB installation.');
 $('device-status').textContent='Allow MIDI access in the browser prompt.';
 let timer;
 try{return await Promise.race([
  navigator.requestMIDIAccess({sysex:true}),
  new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('MIDI permission is still pending. Allow the browser prompt, then retry.')),20000);})
 ]);}finally{clearTimeout(timer);}
}
function readableError(e){return e.name==='NotAllowedError'?'MIDI access was blocked. Allow MIDI for this site in Chrome or Edge, then retry.':e.message;}
function confirmGeneric(identity){
 return new Promise(resolve=>{
  const dialog=$('confirm-loader');$('loader-identity').textContent=identity;
  dialog.returnValue='cancel';dialog.addEventListener('close',()=>resolve(dialog.returnValue==='fm1'),{once:true});dialog.showModal();
 });
}
document.addEventListener('nesynth-loading',e=>{loading=e.detail;controls();});
document.addEventListener('nesynth-load-error',e=>message(e.detail,true));
document.addEventListener('nesynth-rom',e=>{
 if(busy)return;
 installed=false;
 rom=new Uint8Array(e.detail.bytes);filename=e.detail.name;prepared=null;lastAction=null;
 $('retry-install').hidden=true;$('build-review').hidden=true;
 if(downloadUrl)URL.revokeObjectURL(downloadUrl);
 $('rom-summary').hidden=false;$('rom-name').textContent=e.detail.title;
 $('rom-info').textContent=`${(rom.length/1024).toFixed(0)} KiB · ${filename}`;
 message(filename==='Original diagnostic'?'The diagnostic is selected. Choose a free game or your own file in step 1, or install this diagnostic.':'Game selected. '+(connectionChecked?'Next: prepare your game in step 3.':'Next: plug in your FM-1 and press Connect FM-1.'));controls();
});
document.addEventListener('nesynth-rom-clear',()=>{if(busy)return;rom=null;prepared=null;lastAction=null;installed=false;if(downloadUrl)URL.revokeObjectURL(downloadUrl);downloadUrl=null;$('retry-install').hidden=true;$('build-review').hidden=true;$('rom-summary').hidden=true;message('Choose a supported game in step 1.');controls();});
$('flash-model').onchange=()=>{lastAction=null;$('retry-install').hidden=true;available();};
$('connect-device').onclick=async()=>{
 if(busy||loading)return;busy=true;operation='connect';connectionChecked=false;controls();progress.start('connect');
 try{
  access=await midi();const up=new Updater(access,log),dev=await up.findAny(fm1Identity);
  if(!dev)throw Error('No FM-1 identity reply. Use a USB data cable, connect only the FM-1, and close other MIDI apps. An active loader can be resumed with Install / resume.');
  try{connectionChecked=true;$('device-status').textContent='Connected: '+(dev.id.text==='FM-1_015'?'M-VAVE factory V15':dev.id.text);log('Connected: '+dev.id.text+' on '+dev.name);}
  finally{await dev.link.close();}
  access.onstatechange=e=>{if(e.port.state==='disconnected'&&!busy){connectionChecked=false;$('device-status').textContent='USB connection changed. Reconnect your FM-1 and check the connection.';controls();}};
  progress.stage('ready','Device connected');message(prepared?'FM-1 connected. Next: press Install game on FM-1 in step 4.':rom?'FM-1 connected. Next: press Prepare game for FM-1 in step 3.':'FM-1 connected. Choose a game in step 1.');
 }catch(e){$('device-status').textContent=readableError(e);message(readableError(e),true);$('connection-help').open=true;log(readableError(e));progress.fail();}
 finally{busy=false;operation='';controls();}
};
$('build-firmware').onclick=async()=>{
 if(busy||loading||!rom)return;
 prepared=null;lastAction=null;$('build-review').hidden=true;$('retry-install').hidden=true;
 if(downloadUrl)URL.revokeObjectURL(downloadUrl);downloadUrl=null;
 busy=true;operation='prepare';installed=false;controls();message('Preparing your game in this browser. Your FM-1 is unchanged.');
 try{
  const response=await fetch('firmware/template.json',{cache:'no-store',signal:AbortSignal.timeout(20000)});if(!response.ok)throw Error('Firmware template is unavailable.');
  const metadata=await response.json(),download=await fetch('firmware/'+metadata.package,{cache:'no-store',signal:AbortSignal.timeout(20000)});
  if(!download.ok)throw Error('Firmware download failed.');
  prepared=await prepareFirmware(rom,new Uint8Array(await download.arrayBuffer()),metadata);
  if(downloadUrl)URL.revokeObjectURL(downloadUrl);
  downloadUrl=URL.createObjectURL(new Blob([prepared.bytes],{type:'application/octet-stream'}));
  $('download-firmware').href=downloadUrl;$('download-firmware').download='nesynth-'+filename.replace(/[^a-z0-9_-]/gi,'_')+'.fwsc';
  $('build-title').textContent=`${$('rom-name').textContent} → FM-1`;
  $('build-details').textContent='Game prepared and verified. Ready to install over USB.';
  $('firmware-version').textContent='Firmware: '+prepared.product;
  $('build-review').hidden=false;message('Game ready. '+(connectionChecked?'Next: press Install game on FM-1 in step 4.':'Plug in your FM-1, then press Install game on FM-1.'));
 }catch(e){prepared=null;message(readableError(e),true);}
 finally{busy=false;operation='';controls();}
};
async function run(action){
 if(busy||loading||$('flash-model').value!=='fm1'||(action==='game'&&!prepared))return;
 const game=prepared;lastAction=action;busy=true;operation=action;installed=false;controls();$('retry-install').hidden=true;
 message(action==='restore'?'Restoring M-VAVE firmware. Keep power and USB connected.':'Installing your game. Keep power and USB connected.');
 $('install-log').textContent='';reveal();progress.start(action==='restore'?'prepare':'connect');
 document.dispatchEvent(new Event('nesynth-pause'));let wake;
 try{
  try{wake=await navigator.wakeLock?.request('screen');}catch{}
  access=await midi();const options={confirmGeneric};
  const product=action==='restore'?await restoreLatest(access,log,options):await installFirmware(access,game.install,log,options);
  installed=action==='game';connectionChecked=true;progress.done();$('device-status').textContent='Restart verified: '+product;
  if(action==='restore'){$('restore-status').textContent='M-VAVE V15 restored and restart verified.';message('Your FM-1 is back on its original M-VAVE firmware.');}
  else message('Installed and restart verified. Your NES cartridge starts automatically on the FM-1.');
 }catch(e){
  connectionChecked=false;const error=readableError(e);log(error);message(error,true);progress.fail();
  if(action==='restore')$('restore-status').textContent=error;
  $('retry-install').hidden=false;$('device-status').textContent='Installation not confirmed. See the log and retry instructions below.';
 }finally{await wake?.release().catch(()=>{});busy=false;operation='';controls();}
}
$('flash-device').onclick=()=>run('game');$('restore-device').onclick=()=>run('restore');
$('retry-install').onclick=()=>{if(lastAction)run(lastAction);};
$('save-install-log').onclick=()=>{
 const a=document.createElement('a'),url=URL.createObjectURL(new Blob([$('install-log').textContent],{type:'text/plain'}));
 a.href=url;a.download='nesynth-install-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
available();
showAvailableVersion();
