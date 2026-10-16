// Real main-page UI with simulated Bluetooth and USB; no hardware access.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const root=path.resolve('web'),types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.wasm':'application/wasm','.svg':'image/svg+xml','.png':'image/png','.json':'application/json'};
const server=http.createServer(async(req,res)=>{try{
 const u=new URL(req.url,'http://localhost'),f=path.resolve(root,'.'+(u.pathname==='/'?'/index.html':u.pathname));
 if(!f.startsWith(root+path.sep))throw Error();res.setHeader('Content-Type',types[path.extname(f)]||'application/octet-stream');res.end(await fs.readFile(f));
}catch{res.statusCode=404;res.end('Not found');}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({channel:'chrome',headless:true});
try{
 const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  const words=xs=>{const b=new Uint8Array(xs.length*4),v=new DataView(b.buffer);xs.forEach((x,i)=>v.setUint32(i*4,x,true));return v;};
  const entry=new Uint8Array(44),ev=new DataView(entry.buffer);ev.setUint32(0,0x3b000,true);ev.setUint32(4,24592,true);ev.setUint32(8,123,true);entry.set(new TextEncoder().encode('A saved game with a long title'),12);
  let query=0,state=0,received=0;window.packets=[];window.sent=[];window.failRead=false;window.usbConnects=0;
  const device=new EventTarget();device.gatt={connected:false,async connect(){this.connected=true;return {getPrimaryService:async()=>({getCharacteristic:async()=>channel})};},disconnect(){this.connected=false;device.dispatchEvent(new Event('gattserverdisconnected'));}};
  async function write(value){
   const p=new Uint8Array(value),v=new DataView(p.buffer,p.byteOffset,p.byteLength);window.packets.push(Array.from(p));query=0;
   if(p[0]===6||p[0]===7)query=p[0];
   if(p[0]===4){state=received=0;window.sent=[];}
   if(p[0]===1){state=1;received=0;}
   if(p[0]===2){
    if(window.holdWrite){window.holdWrite=false;await new Promise(r=>window.releaseWrite=r);}
    if(v.getUint32(1,true)!==received)throw Error('Wrong transfer offset');window.sent.push(...p.slice(5));received+=p.length-5;
   }
   if(p[0]===3)state=2;
  }
  const channel={service:{device},properties:{writeWithoutResponse:true},writeValueWithResponse:write,writeValueWithoutResponse:write,async readValue(){
   if(window.failRead)throw Error('Transient read failure');
   if(query===6)return words([0x324c504e,2,1,372736,401408,262672,262672,32,1,0]);
   if(query===7)return new DataView(entry.buffer);
   return words([0x314c504e,state,0,received,262672]);
  }};
  window.fakeConnect=async usb=>{device.transport=usb?'USB':undefined;channel.properties.writeWithoutResponse=!usb;query=state=received=0;device.gatt.connected=true;return {device,characteristic:channel};};
  Object.defineProperty(navigator,'bluetooth',{configurable:true,value:{requestDevice:async options=>{if(options.filters[0].services[0]!=='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f0')throw Error('Wrong BLE service');await window.fakeConnect(false);return device;}}});
  Object.defineProperty(navigator,'requestMIDIAccess',{value:async()=>{throw Error('Unexpected firmware MIDI access');}});
 });
 await page.route('**/usb-library.js',r=>r.fulfill({contentType:'text/javascript',body:'export async function connectUsbLibrary(){window.usbConnects++;return window.fakeConnect(true);}'}));
 await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>!document.querySelector('#dropzone').disabled);
 assert.equal(await page.locator('input[accept*=".nes"]').count(),1,'One shared cartridge picker');
 assert.match(await page.locator('#device-name').textContent(),/Original diagnostic/);
 assert(await page.locator('#flash-device').isHidden());
 await page.locator('#firmware-tab').click();
 assert(await page.locator('#firmware-panel').isVisible());
 assert(await page.locator('#games-panel').isHidden());
 await page.locator('#firmware-tab').press('ArrowRight');
 assert.equal(await page.locator('#games-tab').getAttribute('aria-selected'),'true');
 await page.locator('#games-tab').press('Home');
 assert.equal(await page.locator('#firmware-tab').getAttribute('aria-selected'),'true');
 await page.locator('#firmware-tab').press('End');
 await page.locator('#device-connect').click();
 await page.waitForFunction(()=>document.querySelector('#device-connection').textContent==='Bluetooth connected');
 assert.match(await page.locator('#device-space').textContent(),/364 KiB free.*1 games/);
 assert.equal(await page.locator('#device-games button').count(),2);
 assert(await page.locator('#flash-device').isDisabled());
 await page.locator('#firmware-tab').click();
 assert(await page.locator('#firmware-lock').isVisible());
 await page.locator('#games-tab').click();
 await page.evaluate(()=>document.querySelector('#connect-device').onclick());
 assert.equal(await page.locator('#device-connection').textContent(),'Bluetooth connected');
 const rom=await fs.readFile('web/diagnostic.nes');
 await page.locator('#firmware-tab').click();
 for(const target of ['#screen','.cartridge-door','#dropzone']){
  const transfer=await page.evaluateHandle(({bytes,name})=>{const d=new DataTransfer();d.items.add(new File([new Uint8Array(bytes)],name));return d;},{bytes:[...rom],name:'Shared drop.nes'});
  await page.locator(target).dispatchEvent('dragenter',{dataTransfer:transfer});
  await page.locator(target).dispatchEvent('drop',{dataTransfer:transfer});await transfer.dispose();
  await page.waitForFunction(()=>document.querySelector('#device-name').textContent.includes('Shared drop.nes')&&!document.querySelector('#device-send').disabled);
  assert.equal(await page.locator('#rom-name').textContent(),'Shared drop.nes');
  assert(await page.locator('#games-panel').isVisible(),'Game drops open Add games');
  assert.equal(await page.locator('#device-connection').textContent(),'Bluetooth connected');
 }
 assert.equal(await page.evaluate(()=>window.packets.filter(p=>[1,2,3,8,9].includes(p[0])).length),0,'Selection never sends a game');
 await page.locator('#rom').setInputFiles({name:'broken.nes',buffer:Buffer.from('bad')});
 await page.waitForFunction(()=>document.querySelector('#status').classList.contains('error'));
 assert(await page.locator('#device-send').isDisabled());
 await page.locator('#rom').setInputFiles('web/diagnostic.nes');
 await page.waitForFunction(()=>!document.querySelector('#device-send').disabled);
 await fs.mkdir('build/main-loader-ui',{recursive:true});
 for(const [width,height,name] of [[1440,1100,'desktop'],[390,844,'mobile']]){
  await page.setViewportSize({width,height});await page.locator('#flash-panel').scrollIntoViewIfNeeded();
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'No horizontal overflow');
  await page.screenshot({path:`build/main-loader-ui/${name}.png`,fullPage:true});
  const row=await page.locator('#device-games li').boundingBox(),buttons=await page.locator('#device-games button').all();
  for(const button of buttons){const b=await button.boundingBox();assert(b.x>=row.x&&b.x+b.width<=row.x+row.width+1,'Library actions fit beside game name');}
 }
 await page.evaluate(()=>window.holdWrite=true);await page.locator('#device-send').click();
 await page.waitForFunction(()=>!!window.releaseWrite);
 assert(await page.locator('#rom').isDisabled());assert(await page.locator('#device-connect').isDisabled());assert(await page.locator('#restore-device').isDisabled());
 await page.evaluate(()=>window.releaseWrite());
 await page.waitForFunction(()=>document.querySelector('#device-loader-status').textContent.includes('Game verified'));
 assert.deepEqual(await page.evaluate(()=>window.sent),[...rom]);
 assert.equal(await page.locator('#device-progress').getAttribute('value'),'1');
 assert.equal(await page.locator('#device-connection').textContent(),'Bluetooth disconnected');
 // Same page works with Bluetooth absent and a USB connection.
 await page.evaluate(()=>Object.defineProperty(navigator,'bluetooth',{value:undefined,configurable:true}));
 await page.locator('#device-transport').selectOption('usb');await page.locator('#device-connect').click();
 await page.waitForFunction(()=>document.querySelector('#device-connection').textContent==='USB connected');
 assert(await page.locator('#device-usb-help').isVisible());assert(await page.locator('#device-bluetooth-help').isHidden());
 await page.locator('#device-send').click();await page.waitForFunction(()=>document.querySelector('#device-connection').textContent==='USB disconnected');
 assert.deepEqual(await page.evaluate(()=>window.sent),[...rom]);
 await page.locator('#device-connect').click();await page.waitForFunction(()=>document.querySelector('#device-games button')&&!document.querySelector('#device-games button').disabled);
 page.once('dialog',d=>d.dismiss());await page.locator('#device-games button').filter({hasText:'Delete'}).click();
 assert.equal(await page.evaluate(()=>window.packets.filter(p=>p[0]===8).length),0);
 page.once('dialog',d=>d.accept());await page.locator('#device-games button').filter({hasText:'Delete'}).click();
 await page.waitForFunction(()=>document.querySelector('#device-connection').textContent==='USB disconnected');
 assert.equal(await page.evaluate(()=>window.packets.filter(p=>p[0]===8).length),1);
 await page.locator('#device-connect').click();await page.waitForFunction(()=>document.querySelector('#device-games button')&&!document.querySelector('#device-games button').disabled);
 await page.locator('#device-games button').filter({hasText:'Play'}).click();
 await page.waitForFunction(()=>document.querySelector('#device-loader-status').textContent.includes('restarting to play'));
 assert.equal(await page.evaluate(()=>window.packets.filter(p=>p[0]===9).length),1);
 const ids=await page.locator('[id]').evaluateAll(ns=>ns.map(n=>n.id));assert.equal(ids.length,new Set(ids).size);
 assert.deepEqual(errors,[]);
 console.log('PASS: main-page shared drops, Bluetooth and USB upload, saved-game actions, free space, firmware exclusion, invalid-file reset, transfer locks and desktop/mobile layout.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
