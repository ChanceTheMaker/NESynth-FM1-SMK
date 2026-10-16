// SPDX-License-Identifier: GPL-3.0-only
import {openCartridge} from './zip.js';
import {decodeLoaderStatus,transferCartridge} from './bluetooth-transfer.js';
import {normalizeCartridge} from './cartridge.js';
import {validateHardwareRom} from './rom-firmware.js';
const SERVICE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f0',VALUE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f1';
const $=s=>document.querySelector(s),status=s=>$('#status').textContent=s;
let device,characteristic,rom,entries=[],busy=false,installing=false;
function refresh(){$('#send').disabled=busy||!rom||!characteristic;$('#connect').disabled=busy;$('#file').disabled=busy;$('#entries').disabled=busy;}
async function state(){return decodeLoaderStatus(await characteristic.readValue());}
async function choose(index){rom=null;refresh();const entry=entries[index],result=await normalizeCartridge(await entry.read());validateHardwareRom(result.bytes);if(result.bytes.length>262672)throw Error('This build holds cartridges up to 262672 bytes. Choose a smaller game.');rom=result.bytes;$('#name').textContent=`${entry.name} / ${rom.length.toLocaleString()} bytes`;status(result.notice||'Game ready. Connect your SMK, then send and play.');refresh();}
async function open(file){if(busy||!file)return;rom=null;refresh();try{entries=await openCartridge(file);$('#entries').replaceChildren(...entries.map((e,i)=>new Option(e.name,String(i))));$('#entries').hidden=entries.length<2;await choose(0);}catch(e){status(e.message);}}
$('#file').onchange=e=>open(e.target.files[0]);$('#entries').onchange=e=>choose(Number(e.target.value)).catch(e=>status(e.message));
for(const event of ['dragenter','dragover'])$('#drop').addEventListener(event,e=>{e.preventDefault();$('#drop').classList.add('over');});
for(const event of ['dragleave','drop'])$('#drop').addEventListener(event,e=>{e.preventDefault();$('#drop').classList.remove('over');if(event==='drop')open(e.dataTransfer.files[0]);});
$('#connect').onclick=async()=>{try{installing=false;if(!navigator.bluetooth)throw Error('Open this page in Chrome or Edge with Bluetooth enabled.');device=await navigator.bluetooth.requestDevice({filters:[{services:[SERVICE]}]});device.addEventListener('gattserverdisconnected',()=>{characteristic=null;refresh();if(!busy&&!installing)status('SMK disconnected. Hold PLAY + REC to load another game.');});const server=await device.gatt.connect();characteristic=await (await server.getPrimaryService(SERVICE)).getCharacteristic(VALUE);await state();status('SMK connected. Choose a game, then send and play.');refresh();}catch(e){characteristic=null;refresh();status(e.name==='NotFoundError'?'No SMK selected. Hold PLAY + REC together for two seconds until the SMK screen says SEND A GAME, then click Connect SMK and select NESynth SMK. If it is missing, check that your computer Bluetooth is on.':e.message);}};
$('#send').onclick=async()=>{
    busy=true;refresh();const started=performance.now();
    try{
        const session=crypto.getRandomValues(new Uint32Array(1))[0]||1;
        const channel=characteristic,burst=!!channel.properties.writeWithoutResponse;
        await transferCartridge({rom,session,burst,read:async()=>decodeLoaderStatus(await channel.readValue()),
            write:(bytes,response)=>response?channel.writeValueWithResponse(bytes):channel.writeValueWithoutResponse(bytes),
            onProgress:(sent,total)=>{$('#progress').value=sent/total;status(`Sending game... ${Math.floor(sent*100/total)}%`);}});
        installing=true;status(`Game verified in ${((performance.now()-started)/1000).toFixed(1)} seconds. Your SMK is restarting to install and play. Keep it powered.`);characteristic=null;
    }catch(e){status(e.message);}finally{busy=false;refresh();}
};
