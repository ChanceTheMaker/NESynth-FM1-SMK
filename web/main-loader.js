// SPDX-License-Identifier: GPL-3.0-only
import {mountDeviceLoader} from './device-loader.js?v=970';
const tabs=[...document.querySelectorAll('.device-tabs [role="tab"]')];
function selectTab(name,focus=false){
    for(const tab of tabs){
        const selected=tab.id===name+'-tab';
        tab.setAttribute('aria-selected',String(selected));tab.tabIndex=selected?0:-1;
        document.getElementById(tab.getAttribute('aria-controls')).hidden=!selected;
        if(selected&&focus)tab.focus();
    }
}
for(const [i,tab] of tabs.entries()){
    tab.addEventListener('click',()=>selectTab(tab.id.replace('-tab','')));
    tab.addEventListener('keydown',e=>{
        const next=e.key==='Home'?0:e.key==='End'?tabs.length-1:e.key==='ArrowRight'?(i+1)%tabs.length:e.key==='ArrowLeft'?(i+tabs.length-1)%tabs.length:null;
        if(next!==null){e.preventDefault();selectTab(tabs[next].id.replace('-tab',''),true);}
    });
}
document.querySelectorAll('[data-device-tab]').forEach(link=>link.addEventListener('click',()=>selectTab(link.dataset.deviceTab)));
function hashTab(){if(['#games-panel','#step-game'].includes(location.hash))selectTab('games');else if(location.hash==='#firmware-panel')selectTab('firmware');}
window.addEventListener('hashchange',hashTab);hashTab();
let previous='';
const loader=mountDeviceLoader({root:document.querySelector('#device-loader'),prefix:'device-',sharedSelection:true,onState:state=>{
    const key=JSON.stringify(state);if(key===previous)return;previous=key;
    document.querySelector('#firmware-lock').hidden=!state.connected&&!state.busy&&!state.checking;
    document.dispatchEvent(new CustomEvent('nesynth-device-state',{detail:state}));
}});
let selected=false;
document.addEventListener('nesynth-rom',e=>{
    loader.setCartridge(e.detail.bytes,e.detail.name);selected=true;
    document.querySelector('#rom-summary').hidden=false;
    document.querySelector('#rom-name').textContent=e.detail.title;
    document.querySelector('#rom-info').textContent=`${Math.round(e.detail.bytes.length/1024)} KiB · ${e.detail.name}`;
    document.querySelector('#game-state').textContent='Selected';
});
document.addEventListener('nesynth-rom-clear',()=>{
    loader.clearCartridge();selected=false;document.querySelector('#rom-summary').hidden=true;
    document.querySelector('#game-state').textContent='Choose a game';
});
document.addEventListener('nesynth-loading',e=>{
    loader.setLoading(e.detail);document.querySelector('#game-state').textContent=e.detail?'Loading…':selected?'Selected':'Choose a game';
    if(e.detail)selectTab('games');
});
document.addEventListener('nesynth-install-busy',e=>loader.setBlocked(e.detail));
