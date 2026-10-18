import {requestMidiSession,releaseMidiSession} from './midi-session.js?v=session1';
import {loadNesynthFirmware} from './nesynth-firmware.js?v=971';
import {Updater} from './vendor/fm1ota.js?v=973';
import {installFirmware,fm1Identity} from './install-firmware.js?v=20261013';
import {restoreLatest} from './restore-firmware.js?v=20261013';
import {createProgress,PROGRESS_EVENT,progressView} from './update-progress.js?v=20261013';
import {SMK_MODELS,getSmkStock,prepareSmkRestore} from './smk-firmware.js?v=963';
import {installSmkFirmware,smkIdentity,SMK_PORT_PATTERN} from './smk-install.js';
import {hasNativeSmk,runNativeSmk,needsHostedSmkHelper} from './native-smk.js?v=975-live-helper';
const $=id=>document.getElementById(id);
const isFm=()=>$('flash-model').value==='fm1';
const modelName=()=>isFm()?'FM-1':SMK_MODELS[$('flash-model').value];
const stockCache=new Map();
async function stockFor(model){if(!stockCache.has(model))stockCache.set(model,await getSmkStock(model));return stockCache.get(model).slice();}
let prepared=null,access=null,busy=false,lastAction=null;
let connectionChecked=false,installed=false,operation='',deviceLocked=false;
const progress=createProgress($('install-log-heading'));
const message=(s,error=false)=>{$('flash-status').textContent=s;$('flash-status').classList.toggle('error',error);};
document.addEventListener(PROGRESS_EVENT,e=>{
 if(!busy||!['firmware','restore'].includes(operation))return;
 const view=progressView(e.detail);
 message(view.label+(e.detail.phase==='transfer'?' — '+view.detail:'. Keep power and USB connected.'));
 $('device-status').textContent=view.label;
});
function controls(){
 const locked=deviceLocked||busy,model=modelName();
 for(const id of ['connect-device','flash-model','restore-device','retry-install'])$(id).disabled=locked;
 $('flash-device').disabled=locked;
 $('connect-device').textContent=operation==='connect'?'Connecting…':connectionChecked?'Check connection again':'Connect '+model;
 $('flash-device').textContent=operation==='prepare'?'Preparing NESynth…':operation==='firmware'?'Installing…':installed?'Reinstall NESynth':'Install NESynth on '+model;
 $('connect-state').textContent=connectionChecked?'Checked':'USB';
 $('install-state').textContent=installed?'Verified':'Not installed';
 const completed=[connectionChecked,installed];
 ['connect','install'].forEach((step,i)=>{
  $('step-'+step).dataset.complete=String(completed[i]);
  $('step-'+step).dataset.current=String(i===completed.indexOf(false));
 });
 $('install-help').textContent=installed?'Restart verified. Use Add games to load your library.':'The correct NESynth firmware is downloaded and verified automatically.';
 document.dispatchEvent(new CustomEvent('nesynth-install-busy',{detail:busy}));
}
function available(){
 const model=modelName(),fm=isFm();
 $('firmware-heading').textContent='Install NESynth on your '+model+'.';
 $('connect-heading').textContent='Connect your '+model;
 $('connect-hint').textContent=fm?'Factory M-VAVE V15 and updater-enabled firmware work here. Close other MIDI apps and unplug other instruments.':'Official M-VAVE 016 and updater-enabled firmware work here. Connect only the selected SMK model and close other MIDI apps.';
 $('restore-copy').textContent='Restore the official M-VAVE '+(fm?'V15':'016')+' firmware for '+model+'. This replaces NESynth and its game library. The download is verified before transfer.';
 $('restore-status').textContent='Connect your '+model+', then press Restore M-VAVE firmware.';
 $('connection-help-title').textContent=model+' not showing up?';
 $('connection-help-copy').textContent='Connect only the '+model+' directly by USB, allow MIDI / SysEx access, and close other MIDI apps or browser tabs. Older builds without a USB updater need the recovery adapter. If NESynth is in Bluetooth mode, restart it before installing firmware.';
 $('smk-helper-help').hidden=!needsHostedSmkHelper($('flash-model').value);
 $('connect-copy').textContent=needsHostedSmkHelper($('flash-model').value)?'Turn it on and connect it directly to this computer with a USB data cable. The USB helper must be running.':'Turn it on and connect it directly to this computer with a USB data cable. Use Chrome or Edge and allow MIDI access. Restart NESynth before a firmware update so Bluetooth is off.';
 if(needsHostedSmkHelper($('flash-model').value))$('connect-hint').textContent='On Windows, start the NESynth USB helper, then connect here. Allow local-network access if Chrome asks. The helper handles USB outside the browser.';
 $('flash-availability').textContent='USB setup · Chrome or Edge on a computer.';
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
 return requestMidiSession();
}
function readableError(e){return e.name==='NotAllowedError'?'MIDI access was blocked. Allow MIDI for this site in Chrome or Edge, then retry.':e.message;}
function confirmGeneric(identity,model='FM-1'){
 return new Promise(resolve=>{
  const dialog=$('confirm-loader');$('loader-identity').textContent=identity;
  $('loader-model').textContent='Continue only if the connected instrument is '+model+'.';$('confirm-model').textContent='It is '+model;dialog.returnValue='cancel';dialog.addEventListener('close',()=>resolve(dialog.returnValue==='confirm'),{once:true});dialog.showModal();
 });
}
document.addEventListener('nesynth-device-state',e=>{deviceLocked=e.detail.busy||e.detail.checking||e.detail.connected;controls();});
$('flash-model').onchange=()=>{prepared=null;lastAction=null;installed=false;connectionChecked=false;$('retry-install').hidden=true;$('device-status').textContent='No device connected.';message('NESynth is ready for '+modelName()+'. Connect by USB, then install.');available();};
$('connect-device').onclick=async()=>{
 if(deviceLocked||busy)return;busy=true;operation='connect';connectionChecked=false;controls();progress.start('connect');
 try{
  if(await hasNativeSmk($('flash-model').value)){
   const identity=await runNativeSmk('connect',log);connectionChecked=true;
   $('device-status').textContent='Connected: '+identity;progress.stage('ready','Device connected');message(modelName()+' connected. Press Install NESynth.');return;
  }
  access=await midi();const up=new Updater(access,log,isFm()?undefined:SMK_PORT_PATTERN),dev=await up.findAny(isFm()?fm1Identity:smkIdentity(modelName()));
  if(!dev)throw Error('No '+modelName()+' identity reply. Use a USB data cable, connect only this instrument, and close other MIDI apps. An active loader can be resumed with Install / resume.');
  try{connectionChecked=true;$('device-status').textContent='Connected: '+(dev.id.text==='FM-1_015'?'M-VAVE factory V15':dev.id.text);log('Connected: '+dev.id.text+' on '+dev.name);}
  finally{await dev.link.close();}
  access.onstatechange=e=>{if(e.port.state==='disconnected'&&!busy){connectionChecked=false;$('device-status').textContent='USB connection changed. Reconnect your '+modelName()+' and check the connection.';controls();}};
  progress.stage('ready','Device connected');message(modelName()+' connected. Press Install NESynth.');
 }catch(e){$('device-status').textContent=readableError(e);message(readableError(e),true);$('connection-help').open=true;log(readableError(e));progress.fail();}
 finally{await releaseMidiSession();access=null;busy=false;operation='';controls();}
};
async function run(action){
 if(deviceLocked||busy||!modelName())return;
 const model=modelName(),fm=isFm();lastAction=action;busy=true;operation=action;installed=false;controls();$('retry-install').hidden=true;
 message(action==='restore'?'Restoring M-VAVE firmware. Keep power and USB connected.':'Installing NESynth firmware. Keep power and USB connected.');
 $('install-log').textContent='';reveal();progress.start(action==='restore'?'prepare':'connect');
 document.dispatchEvent(new Event('nesynth-pause'));let wake;
 try{
  try{wake=await navigator.wakeLock?.request('screen');}catch{}
  if(await hasNativeSmk($('flash-model').value)){
   const product=await runNativeSmk(action==='restore'?'restore':'install',log);
   const expected=action==='restore'?'SMK-37 Pro_016':'SMK-37_970';
   if(product!==expected)throw Error('Unexpected firmware after restart: '+product+'; expected '+expected+'.');
   installed=action==='firmware';connectionChecked=true;progress.done();$('device-status').textContent='Restart verified: '+product;
   if(action==='restore'){$('restore-status').textContent='M-VAVE 016 restored and restart verified.';message('Your '+model+' is back on its original M-VAVE firmware.');}
   else message('NESynth installed and restart verified on '+model+'. Open Add games to load your library.');
   return;
  }
  if(action==='firmware'&&!prepared){
   operation='prepare';controls();progress.stage('prepare','Preparing NESynth firmware');message('Downloading and verifying NESynth for '+model+'…');
   prepared=await loadNesynthFirmware($('flash-model').value);log('NESynth core verified: '+prepared.product);operation='firmware';controls();message('NESynth verified. Connecting to '+model+'…');
  }
  access=await midi();$('device-status').textContent='USB access granted. Finding '+model+'…';const options={confirmGeneric};
  let product;if(fm)product=action==='restore'?await restoreLatest(access,log,options):await installFirmware(access,prepared.install,log,options);else{const handle=action==='restore'?await prepareSmkRestore(await stockFor(model),model):prepared.install;product=await installSmkFirmware(access,handle,log,options);}
  installed=action==='firmware';connectionChecked=true;progress.done();$('device-status').textContent='Restart verified: '+product;
  if(action==='restore'){$('restore-status').textContent='M-VAVE '+(fm?'V15':'016')+' restored and restart verified.';message('Your '+model+' is back on its original M-VAVE firmware.');}
  else message('NESynth installed and restart verified on '+model+'. Open Add games to load your library.');
 }catch(e){
  connectionChecked=false;const error=readableError(e);log(error);message(error,true);progress.fail();
  if(action==='restore')$('restore-status').textContent=error;
  $('retry-install').hidden=false;$('device-status').textContent='Installation not confirmed. See the log and retry instructions below.';
 }finally{await releaseMidiSession();access=null;await wake?.release().catch(()=>{});busy=false;operation='';controls();}
}
$('flash-device').onclick=()=>run('firmware');$('restore-device').onclick=()=>run('restore');
$('retry-install').onclick=()=>{if(lastAction)run(lastAction);};
$('save-install-log').onclick=()=>{
 const a=document.createElement('a'),url=URL.createObjectURL(new Blob([$('install-log').textContent],{type:'text/plain'}));
 a.href=url;a.download='nesynth-install-log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
available();
