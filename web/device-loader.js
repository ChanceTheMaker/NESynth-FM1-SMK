// SPDX-License-Identifier: GPL-3.0-only
import {connectUsbLibrary} from './usb-library.js?v=midi-session1';
import {openCartridge} from './zip.js';
import {decodeLoaderStatus,transferCartridge,loaderPacket} from './bluetooth-transfer.js';
import {readLibrary,gameNamePacket,ensureLibrarySpace} from './bluetooth-library.js';
import {normalizeCartridge} from './cartridge.js';
import {validateHardwareRom} from './rom-firmware.js?v=969';
const SERVICE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f0',VALUE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f1';
// One transport/library controller for the standalone and main-page views.
export function mountDeviceLoader({root=document,prefix='',sharedSelection=false,onState=()=>{}}={}){
const selector=s=>s.replace(/#([a-z][a-z0-9-]*)/gi,(_,id)=>'#'+prefix+(sharedSelection&&id==='status'?'loader-status':id));
const $=s=>root.querySelector(selector(s)),status=s=>$('#status').textContent=s;
let blocked=false,externalLoading=false;
let device,characteristic,rom,romName='',entries=[],busy=false,checking=false,loading=false,dragging=false,connectionIssue=false,installing=false,library=null;
function connectionName(){return device?.transport||($('#transport').value==='usb'?'USB':'Bluetooth');}
function refresh(){
    const locked=busy||checking||loading||externalLoading||blocked,connected=!!characteristic&&!!device?.gatt.connected;
    $('#send').disabled=locked||!rom||!connected;$('#connect').disabled=locked;if(!sharedSelection){$('#file').disabled=busy||loading||blocked;$('#entries').disabled=locked;}
    $('#transport').disabled=locked||connected;
    $('#bluetooth-help').hidden=$('#transport').value==='usb';$('#usb-help').hidden=$('#transport').value!=='usb';
    $('#connection').textContent=`${connectionName()} ${connected?(connectionIssue?'connection unconfirmed':'connected'):'disconnected'}`;
    $('#connect').textContent=connected?'Disconnect device':'Connect device';
    root.querySelectorAll(selector('#library button')).forEach(b=>b.disabled=locked||!connected);
    onState({busy,checking,connected});
}
function disconnected(current){
    if(current!==device)return;characteristic=null;connectionIssue=false;library=null;$('#library').hidden=true;refresh();
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
    if(busy||loading||checking||blocked||externalLoading||!characteristic)return;
    if(op===8&&!confirm(`Delete ${game.name} from the device?`))return;
    busy=true;refresh();try{
        await characteristic.writeValueWithResponse(loaderPacket(op,game.index,game.crc));installing=true;
        status(op===8?`Device is restarting to delete the game. ${connectionName()==='USB'?'Reconnect using USB.':'Reconnect when SEND A GAME appears.'}`:'Device is restarting to play your game.');
        device.gatt.disconnect();disconnected(device);
    }catch(e){status(e.message);}finally{busy=false;refresh();}
}
async function choose(index){
    const entry=entries[index],result=await normalizeCartridge(await entry.read());
    validateHardwareRom(result.bytes);
    if(result.bytes.length>262672)throw Error('This build holds individual games up to 262672 bytes. Choose a smaller game.');
    rom=result.bytes;romName=entry.name;$('#name').textContent=`${entry.name} / ${rom.length.toLocaleString()} bytes`;
    status(result.notice||(characteristic&&device?.gatt.connected?'Game ready. Click Send and play.':'Game ready. Connect your device, then send and play.'));
}
// Importing is local work: accept files even during a connection check, but keep
// uploads and competing imports locked until validation has finished.
async function importGame(file,index){
    if(busy||loading||blocked)return;
    loading=true;rom=null;$('#name').textContent='Reading game...';refresh();
    try{
        if(file){
            entries=await openCartridge(file);
            $('#entries').replaceChildren(...entries.map((e,i)=>new Option(e.name,String(i))));
            $('#entries').hidden=entries.length<2;$('#entries').value='0';
        }
        await choose(index);
    }catch(e){$('#name').textContent='No game selected';status(e.message);}
    finally{loading=false;refresh();}
}
if(!sharedSelection){
$('#file').onchange=e=>e.target.files[0]&&importGame(e.target.files[0],0);
$('#entries').onchange=e=>importGame(null,Number(e.target.value));
const fileDrag=e=>Array.from(e.dataTransfer?.types||[]).includes('Files')||e.dataTransfer?.files?.length;
// Catch file drops across the page so a miss or a child element cannot navigate
// away and tear down the USB/Bluetooth connection. Only the box selects a game.
for(const event of ['dragenter','dragover'])document.addEventListener(event,e=>{
    if(!fileDrag(e))return;e.preventDefault();dragging=true;
    $('#drop').classList.add('over');
});
document.addEventListener('dragleave',e=>{
    if(e.relatedTarget)return;dragging=false;$('#drop').classList.remove('over');
});
document.addEventListener('drop',e=>{
    if(!fileDrag(e))return;e.preventDefault();dragging=false;$('#drop').classList.remove('over');
    if($('#drop').contains(e.target)&&e.dataTransfer.files[0])return importGame(e.dataTransfer.files[0],0);
});
}else{
    // Main-page drops are owned by the shared simulator/file picker.
    for(const event of ['dragenter','dragover'])document.addEventListener(event,e=>{if(Array.from(e.dataTransfer?.types||[]).includes('Files'))dragging=true;},true);
    for(const event of ['drop','dragend'])document.addEventListener(event,()=>{dragging=false;},true);
    document.addEventListener('dragleave',e=>{if(!e.relatedTarget)dragging=false;},true);
}
$('#transport').onchange=()=>{device=null;refresh();};
$('#connect').onclick=async()=>{
    if(busy||loading||checking||blocked||externalLoading)return;
    if(device?.gatt.connected){device.gatt.disconnect();disconnected(device);return;}
    busy=true;connectionIssue=false;installing=false;refresh();
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
    }catch(e){device?.gatt.disconnect();disconnected(device);status(e.name==='NotFoundError'?'No device selected. Hold PLAY + REC for the library, then press PLAY (Select). Wait for SEND A GAME and select NESynth SMK or NESynth FM1. Check that computer Bluetooth is on.':e.message);}
    finally{busy=false;refresh();}
};
$('#send').onclick=async()=>{
    if(busy||loading||checking||blocked||externalLoading||!characteristic||!rom)return;busy=true;$('#progress').value=0;refresh();const started=performance.now();
    try{
        const channel=characteristic;ensureLibrarySpace(library,rom);
        if(library){await channel.writeValueWithResponse(Uint8Array.of(4));await channel.writeValueWithResponse(gameNamePacket(romName));}
        const session=crypto.getRandomValues(new Uint32Array(1))[0]||1;
        await transferCartridge({rom,session,burst:!!channel.properties.writeWithoutResponse,read:async()=>decodeLoaderStatus(await channel.readValue()),
            write:(bytes,response)=>response?channel.writeValueWithResponse(bytes):channel.writeValueWithoutResponse(bytes),
            onProgress:(sent,total)=>{$('#progress').value=sent/total;status(`Sending game... ${Math.floor(sent*100/total)}%`);}});
        installing=true;status(`Game verified in ${((performance.now()-started)/1000).toFixed(1)} seconds. Device is restarting to save and play. Keep it powered.`);
        device.gatt.disconnect();disconnected(device);
    }catch(e){status(e.message);}finally{busy=false;refresh();}
};
// Native disconnect events remain authoritative. A delayed/rejected idle read
// must not close a live connection (for example while dragging from Explorer).
setInterval(async()=>{
    if(busy||loading||checking||externalLoading||blocked||dragging||document.hidden||!document.hasFocus()||!characteristic)return;
    const current=device,channel=characteristic;checking=true;refresh();
    try{
        // Keep exactly one read in flight. Racing a separate JS timeout leaves
        // an uncancelled GATT read behind and can conflict with the next command.
        await channel.readValue();
        if(current===device)connectionIssue=false;
    }catch(e){
        if(current===device){
            if(!current.gatt.connected)disconnected(current);
            else connectionIssue=true;
        }
    }finally{checking=false;refresh();}
},4000);
refresh();

return {
    setCartridge(bytes,name){
        if(busy)return;rom=null;romName='';$('#progress').value=0;
        try{
            validateHardwareRom(bytes);
            if(bytes.length>262672)throw Error('This device loader accepts games up to 256 KiB plus cartridge header. Choose a smaller game.');
            rom=new Uint8Array(bytes);romName=name;$('#name').textContent=`${name} / ${rom.length.toLocaleString()} bytes`;
            status(characteristic&&device?.gatt.connected?'Game ready. Click Send and play.':'Game ready. Connect your device, then send and play.');
        }catch(e){$('#name').textContent='Selected game cannot be sent';status(e.message);}
        refresh();
    },
    clearCartridge(){if(busy)return;rom=null;romName='';$('#name').textContent='Choose a game above';$('#progress').value=0;refresh();},
    setLoading(value){if(externalLoading===value)return;externalLoading=value;refresh();},
    setBlocked(value){if(blocked===value)return;blocked=value;refresh();}
};
}
