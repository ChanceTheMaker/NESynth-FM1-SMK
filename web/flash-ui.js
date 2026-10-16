import {prepareFirmware} from './rom-firmware.js?v=961';
import {Updater} from './vendor/fm1ota.js?v=20261013';
import {installFirmware,fm1Identity} from './install-firmware.js?v=20261013';
import {restoreLatest} from './restore-firmware.js?v=20261013';
import {createProgress} from './update-progress.js?v=20261013';
import {SMK_MODELS,getSmkStock,prepareSmkFirmware,prepareSmkRestore} from './smk-firmware.js?v=960';
import {installSmkFirmware,smkIdentity,SMK_PORT_PATTERN} from './smk-install.js';
const $=id=>document.getElementById(id);
const isFm=()=>$('flash-model').value==='fm1';
const modelName=()=>isFm()?'FM-1':SMK_MODELS[$('flash-model').value];
const stockCache=new Map();
async function stockFor(model){if(!stockCache.has(model))stockCache.set(model,await getSmkStock(model));return stockCache.get(model).slice();}
const metadataPath=()=>isFm()?'firmware/template.json':'firmware/smk37-template.json';
let rom=null,filename='',prepared=null,access=null,busy=false,downloadUrl=null,lastAction=null,loading=false;
let connectionChecked=false,installed=false,operation='';
const progress=createProgress($('install-log-heading'));
const message=(s,error=false)=>{$('flash-status').textContent=s;$('flash-status').classList.toggle('error',error);};
async function showAvailableVersion(){
 const selected=$('flash-model').value;
 try{
  const response=await fetch(metadataPath(),{cache:'no-store'});
  if(!response.ok)throw Error('Template unavailable');
  const metadata=await response.json();
  if(!/^(FM-1|SMK-37)_\d+$/.test(metadata.product))throw Error('Unknown firmware version');
  if(!prepared&&$('flash-model').value===selected)$('firmware-version').textContent='Firmware: '+metadata.product;
 }catch{if(!prepared&&$('flash-model').value===selected)$('firmware-version').textContent='Firmware version shown after preparation.';}
}

function controls(){
 const model=modelName();
 $('fm1-steps').hidden=false;
 $('firmware-version').hidden=false;
 $('build-review').hidden=!prepared;
 $('build-firmware').disabled=busy||loading||!rom;$('connect-device').disabled=busy||loading;
 $('flash-device').disabled=busy||loading||!prepared;$('flash-model').disabled=busy||loading;
 $('restore-device').disabled=busy||loading;$('retry-install').disabled=busy||loading;
 $('build-firmware').textContent=operation==='prepare'?'Preparing game…':prepared?'Prepare game again':'Prepare game for '+model;
 $('connect-device').textContent=operation==='connect'?'Connecting…':connectionChecked?'Check connection again':'Connect '+model;
 $('flash-device').textContent=operation==='game'?'Installing…':installed?'Install game again':'Install game on '+model;
 $('game-state').textContent=loading?'Loading…':rom?'Selected':'Choose a game';
 $('connect-state').textContent=connectionChecked?'Checked':'USB';
 $('prepare-state').textContent=prepared?'Ready':'Not prepared';
 $('install-state').textContent=installed?'Verified':'Not installed';
 const completed=[!!rom,connectionChecked,!!prepared,installed];
 ['game','connect','prepare','install'].forEach((step,i)=>{
  $('step-'+step).dataset.complete=String(completed[i]);
  $('step-'+step).dataset.current=String(i===completed.indexOf(false));
 });
 $('install-help').textContent=installed?'Restart verified. Your game is running on '+model+'.':!rom?'Choose a game in step 1 to get started.':!prepared?'Press Prepare game for '+model+' in step 3 to enable installation.':connectionChecked?'Ready. Press Install game on '+model+' to transfer your game.':'Plug in your '+model+'. Install will check the connection and request MIDI permission.';
 document.dispatchEvent(new CustomEvent('nesynth-install-busy',{detail:busy}));
}
function available(){
 const model=modelName(),fm=isFm();
 $('firmware-heading').textContent='Install on your '+model+'.';
 $('connect-heading').textContent='Connect your '+model;
 $('deck-intro').textContent='Choose a game, plug in your '+model+', and install it here. No downloads or compiler needed.';
 $('connect-copy').textContent='Turn it on and connect it directly to this computer with a USB data cable. Use Chrome or Edge and allow MIDI access.';
 $('connect-hint').textContent=fm?'Factory M-VAVE V15 works here. Disconnect the XIAO recovery adapter. Close other MIDI apps and unplug other instruments.':'First install: official M-VAVE 016. Updater-enabled Felucca, Sloop, Melodee, Doom and NESynth also work. Connect only the selected SMK model and close other MIDI apps.';
 $('prepare-copy').textContent='This makes the game ready for your '+model+' in this browser. Your instrument is changed only when you press Install.';
 $('install-copy').textContent='Installing replaces the synth application with this game. Keep power and USB connected until the restart is verified. Device game saves last until restart.';
 $('restore-copy').textContent='Restore the official M-VAVE '+(fm?'V15':'016')+' firmware for '+model+'. The file is downloaded and verified before any transfer.';
 $('restore-status').textContent='Connect your '+model+', then press Restore M-VAVE firmware.';
 $('connection-help-title').textContent=model+' not showing up?';
 $('connection-help-copy').textContent='Connect only the '+model+' directly by USB, allow MIDI / SysEx access, and close other MIDI apps or browser tabs. Then check the connection again. Older custom builds without a USB updater need the recovery adapter.';
 $('flash-availability').textContent=fm?'USB installation available · Chrome or Edge on a computer.':'Experimental SMK candidate · packaging and updater simulations pass; physical playback and restart have not yet been verified. Factory restore is available below.';
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
function confirmGeneric(identity,model='FM-1'){
 return new Promise(resolve=>{
  const dialog=$('confirm-loader');$('loader-identity').textContent=identity;
  $('loader-model').textContent='Continue only if the connected instrument is '+model+'.';$('confirm-model').textContent='It is '+model;dialog.returnValue='cancel';dialog.addEventListener('close',()=>resolve(dialog.returnValue==='confirm'),{once:true});dialog.showModal();
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
 message(filename==='Original diagnostic'?'The diagnostic is selected. Choose a free game or your own file in step 1, or install this diagnostic.':'Game selected. '+(connectionChecked?'Next: prepare your game in step 3.':'Next: plug in your '+modelName()+' and press Connect.'));controls();
});
document.addEventListener('nesynth-rom-clear',()=>{if(busy)return;rom=null;prepared=null;lastAction=null;installed=false;if(downloadUrl)URL.revokeObjectURL(downloadUrl);downloadUrl=null;$('retry-install').hidden=true;$('build-review').hidden=true;$('rom-summary').hidden=true;message('Choose a supported game in step 1.');controls();});
$('flash-model').onchange=()=>{prepared=null;lastAction=null;installed=false;connectionChecked=false;if(downloadUrl)URL.revokeObjectURL(downloadUrl);downloadUrl=null;$('retry-install').hidden=true;$('device-status').textContent='No device connected.';message('Selected '+modelName()+'. Check its connection, then prepare your game.');available();showAvailableVersion();};
$('connect-device').onclick=async()=>{
 if(busy||loading)return;busy=true;operation='connect';connectionChecked=false;controls();progress.start('connect');
 try{
  access=await midi();const up=new Updater(access,log,isFm()?undefined:SMK_PORT_PATTERN),dev=await up.findAny(isFm()?fm1Identity:smkIdentity(modelName()));
  if(!dev)throw Error('No '+modelName()+' identity reply. Use a USB data cable, connect only this instrument, and close other MIDI apps. An active loader can be resumed with Install / resume.');
  try{connectionChecked=true;$('device-status').textContent='Connected: '+(dev.id.text==='FM-1_015'?'M-VAVE factory V15':dev.id.text);log('Connected: '+dev.id.text+' on '+dev.name);}
  finally{await dev.link.close();}
  access.onstatechange=e=>{if(e.port.state==='disconnected'&&!busy){connectionChecked=false;$('device-status').textContent='USB connection changed. Reconnect your '+modelName()+' and check the connection.';controls();}};
  progress.stage('ready','Device connected');message(modelName()+' connected. '+(prepared?'Next: press Install in step 4.':rom?'Next: press Prepare game in step 3.':'Choose a game in step 1.'));
 }catch(e){$('device-status').textContent=readableError(e);message(readableError(e),true);$('connection-help').open=true;log(readableError(e));progress.fail();}
 finally{busy=false;operation='';controls();}
};
$('build-firmware').onclick=async()=>{
 if(busy||loading||!rom)return;
 prepared=null;lastAction=null;$('build-review').hidden=true;$('retry-install').hidden=true;
 if(downloadUrl)URL.revokeObjectURL(downloadUrl);downloadUrl=null;
 busy=true;operation='prepare';installed=false;controls();message('Preparing your game in this browser. Your instrument is unchanged.');
 try{
  const response=await fetch(metadataPath(),{cache:'no-store',signal:AbortSignal.timeout(20000)});if(!response.ok)throw Error('Firmware template is unavailable.');
  const metadata=await response.json(),download=await fetch('firmware/'+metadata.package,{cache:'no-store',signal:AbortSignal.timeout(20000)});
  if(!download.ok)throw Error('Firmware download failed.');
  const template=new Uint8Array(await download.arrayBuffer());prepared=isFm()?await prepareFirmware(rom,template,metadata):await prepareSmkFirmware(rom,template,metadata,await stockFor(modelName()),modelName());
  if(downloadUrl)URL.revokeObjectURL(downloadUrl);
  downloadUrl=URL.createObjectURL(new Blob([prepared.bytes],{type:'application/octet-stream'}));
  $('download-firmware').href=downloadUrl;$('download-firmware').download='nesynth-'+filename.replace(/[^a-z0-9_-]/gi,'_')+'.fwsc';
  $('build-title').textContent=`${$('rom-name').textContent} → ${modelName()}`;
  $('build-details').textContent='Game prepared and verified. Ready to install over USB.';
  $('firmware-version').textContent='Firmware: '+prepared.product;
  $('build-review').hidden=false;message('Game ready. '+(connectionChecked?'Next: press Install in step 4.':'Plug in your '+modelName()+', then press Install.'));
 }catch(e){prepared=null;message(readableError(e),true);}
 finally{busy=false;operation='';controls();}
};
async function run(action){
 if(busy||loading||!modelName()||(action==='game'&&!prepared))return;
 const game=prepared,model=modelName(),fm=isFm();lastAction=action;busy=true;operation=action;installed=false;controls();$('retry-install').hidden=true;
 message(action==='restore'?'Restoring M-VAVE firmware. Keep power and USB connected.':'Installing your game. Keep power and USB connected.');
 $('install-log').textContent='';reveal();progress.start(action==='restore'?'prepare':'connect');
 document.dispatchEvent(new Event('nesynth-pause'));let wake;
 try{
  try{wake=await navigator.wakeLock?.request('screen');}catch{}
  access=await midi();const options={confirmGeneric};
  let product;if(fm)product=action==='restore'?await restoreLatest(access,log,options):await installFirmware(access,game.install,log,options);else{const handle=action==='restore'?await prepareSmkRestore(await stockFor(model),model):game.install;product=await installSmkFirmware(access,handle,log,options);}
  installed=action==='game';connectionChecked=true;progress.done();$('device-status').textContent='Restart verified: '+product;
  if(action==='restore'){$('restore-status').textContent='M-VAVE '+(fm?'V15':'016')+' restored and restart verified.';message('Your '+model+' is back on its original M-VAVE firmware.');}
  else message('Installed and restart verified. Your NES cartridge starts automatically on '+model+'.');
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
