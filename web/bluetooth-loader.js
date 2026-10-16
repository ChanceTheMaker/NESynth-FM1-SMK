// SPDX-License-Identifier: GPL-3.0-only
import {connectUsbLibrary} from './usb-library.js';
import {openCartridge} from './zip.js';
import {decodeLoaderStatus,transferCartridge,loaderPacket} from './bluetooth-transfer.js';
import {readLibrary,gameNamePacket,ensureLibrarySpace} from './bluetooth-library.js';
import {normalizeCartridge} from './cartridge.js';
import {validateHardwareRom} from './rom-firmware.js';
const SERVICE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f0',VALUE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f1';
const $=s=>document.querySelector(s),status=s=>$('#status').textContent=s;
let device,characteristic,rom,romName='',entries=[],busy=false,checking=false,installing=false,library=null;
function connectionName(){return device?.transport||($('#transport').value==='usb'?'USB':'Bluetooth');}
function refresh(){
    const locked=busy||checking,connected=!!characteristic&&!!device?.gatt.connected;
    $('#send').disabled=locked||!rom||!connected;$('#connect').disabled=locked;$('#file').disabled=locked;$('#entries').disabled=locked;
    $('#transport').disabled=locked||connected;
    $('#bluetooth-help').hidden=$('#transport').value==='usb';$('#usb-help').hidden=$('#transport').value!=='usb';
    $('#connection').textContent=`${connectionName()} ${connected?'connected':'disconnected'}`;
    $('#connect').textContent=connected?'Disconnect SMK':'Connect SMK';
    document.querySelectorAll('#library button').forEach(b=>b.disabled=locked||!connected);
}
function disconnected(current){
    if(current!==device)return;characteristic=null;library=null;$('#library').hidden=true;refresh();
    if(!busy&&!installing)status(`${connectionName()} disconnected. Choose your connection and reconnect to load another game.`);
}
function renderLibrary(){
    $('#library').hidden=!library;if(!library)return;
    $('#space').textContent=`${(library.free/1024).toFixed(0)} KiB free of ${(library.total/1024).toFixed(0)} KiB / ${library.count} games`;
    $('#storage').max=library.total;$('#storage').value=library.total-library.free;
    $('#games').replaceChildren();
    for(const game of library.games){
        const row=document.createElement('li'),name=document.createElement('span');name.textContent=`${game.name} / ${Math.ceil(game.bytes/1024)} KiB`;name.title=game.name;row.append(name);
        for(const [label,op] of [['Play',9],['Delete',8]]){const b=document.createElement('button');b.textContent=label;b.onclick=()=>libraryAction(op,game);row.append(b);}$('#games').append(row);
    }
    if(!library.count){const row=document.createElement('li');row.textContent='No games installed yet.';$('#games').append(row);}refresh();
}
async function libraryAction(op,game){
    if(busy||checking||!characteristic)return;
    if(op===8&&!confirm(`Delete ${game.name} from the SMK?`))return;
    busy=true;refresh();try{
        await characteristic.writeValueWithResponse(loaderPacket(op,game.index,game.crc));installing=true;
        status(op===8?'SMK is restarting to delete the game. Reconnect when SEND A GAME appears.':'SMK is restarting to play your game.');
        device.gatt.disconnect();disconnected(device);
    }catch(e){status(e.message);}finally{busy=false;refresh();}
}
async function choose(index){rom=null;refresh();const entry=entries[index],result=await normalizeCartridge(await entry.read());validateHardwareRom(result.bytes);if(result.bytes.length>262672)throw Error('This build holds individual games up to 262672 bytes. Choose a smaller game.');rom=result.bytes;romName=entry.name;$('#name').textContent=`${entry.name} / ${rom.length.toLocaleString()} bytes`;status(result.notice||'Game ready. Connect your SMK, then send and play.');refresh();}
async function open(file){if(busy||checking||!file)return;rom=null;refresh();try{entries=await openCartridge(file);$('#entries').replaceChildren(...entries.map((e,i)=>new Option(e.name,String(i))));$('#entries').hidden=entries.length<2;await choose(0);}catch(e){status(e.message);}}
$('#file').onchange=e=>open(e.target.files[0]);$('#entries').onchange=e=>choose(Number(e.target.value)).catch(e=>status(e.message));
for(const event of ['dragenter','dragover'])$('#drop').addEventListener(event,e=>{e.preventDefault();$('#drop').classList.add('over');});
for(const event of ['dragleave','drop'])$('#drop').addEventListener(event,e=>{e.preventDefault();$('#drop').classList.remove('over');if(event==='drop')open(e.dataTransfer.files[0]);});
$('#transport').onchange=()=>{device=null;refresh();};
$('#connect').onclick=async()=>{
    if(busy||checking)return;
    if(device?.gatt.connected){device.gatt.disconnect();disconnected(device);return;}
    busy=true;installing=false;refresh();
    try{
        if($('#transport').value==='usb'){
            const usb=await connectUsbLibrary(status);device=usb.device;characteristic=usb.characteristic;
            const selected=device;selected.addEventListener('gattserverdisconnected',()=>disconnected(selected));
        }else{
            if(!navigator.bluetooth)throw Error('Choose USB or enable Bluetooth in Chrome or Edge.');
            const selected=await navigator.bluetooth.requestDevice({filters:[{services:[SERVICE]}]});device=selected;
            selected.addEventListener('gattserverdisconnected',()=>disconnected(selected));
            const server=await selected.gatt.connect();characteristic=await (await server.getPrimaryService(SERVICE)).getCharacteristic(VALUE);
        }
        decodeLoaderStatus(await characteristic.readValue());library=await readLibrary(characteristic);renderLibrary();
        status(library?`${connectionName()} connected. Add a game or play one from your library.`:'Bluetooth connected. This older firmware stores one game at a time.');
    }catch(e){device?.gatt.disconnect();disconnected(device);status(e.name==='NotFoundError'?'No SMK selected. On builds 967 and newer, hold PLAY + REC for the library, then press REC. Wait for SEND A GAME and select NESynth SMK. Check that computer Bluetooth is on.':e.message);}
    finally{busy=false;refresh();}
};
$('#send').onclick=async()=>{
    if(busy||checking||!characteristic)return;busy=true;refresh();const started=performance.now();
    try{
        const channel=characteristic;ensureLibrarySpace(library,rom);
        if(library){await channel.writeValueWithResponse(Uint8Array.of(4));await channel.writeValueWithResponse(gameNamePacket(romName));}
        const session=crypto.getRandomValues(new Uint32Array(1))[0]||1;
        await transferCartridge({rom,session,burst:!!channel.properties.writeWithoutResponse,read:async()=>decodeLoaderStatus(await channel.readValue()),
            write:(bytes,response)=>response?channel.writeValueWithResponse(bytes):channel.writeValueWithoutResponse(bytes),
            onProgress:(sent,total)=>{$('#progress').value=sent/total;status(`Sending game... ${Math.floor(sent*100/total)}%`);}});
        installing=true;status(`Game verified in ${((performance.now()-started)/1000).toFixed(1)} seconds. SMK is restarting to save and play. Keep it powered.`);
        device.gatt.disconnect();disconnected(device);
    }catch(e){status(e.message);}finally{busy=false;refresh();}
};
// Read-only heartbeat confirms the radio link, rather than trusting a stale label.
setInterval(async()=>{
    if(busy||checking||!characteristic)return;
    const current=device,channel=characteristic;checking=true;refresh();let timer;
    try{await Promise.race([channel.readValue(),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Game-loader connection timed out.')),5000);})]);}
    catch(e){if(current===device){current.gatt.disconnect();disconnected(current);status(`${connectionName()} connection lost. Check your cable or Bluetooth, then reconnect.`);}}
    finally{clearTimeout(timer);checking=false;refresh();}
},4000);
refresh();
