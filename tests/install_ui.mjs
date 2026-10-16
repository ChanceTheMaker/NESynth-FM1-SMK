// Browser UI regression checks. All MIDI is simulated; no device is accessed.
// PLAYWRIGHT_MODULE may point at an existing Playwright installation.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
execFileSync('python',['tests/archives.py'],{stdio:'pipe'});
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const root=path.resolve('web'),types={'.html':'text/html','.js':'text/javascript','.css':'text/css','.wasm':'application/wasm','.svg':'image/svg+xml','.png':'image/png','.json':'application/json'};
const server=http.createServer(async(req,res)=>{
 try{
  const url=new URL(req.url,'http://localhost'),file=path.resolve(root,'.'+(url.pathname==='/'?'/index.html':url.pathname));
  if(!file.startsWith(root+path.sep))throw Error('Invalid path');
  res.setHeader('Content-Type',types[path.extname(file)]||'application/octet-stream');res.end(await fs.readFile(file));
 }catch{res.statusCode=404;res.end('Not found');}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const browser=await chromium.launch({channel:'chrome',headless:true});
try{
 const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  window.midiWrites=0;window.denyMidi=false;
  Object.defineProperty(navigator,'requestMIDIAccess',{value:async()=>{
   if(window.denyMidi)throw new DOMException('Blocked','NotAllowedError');
   return {inputs:new Map(),outputs:new Map()};
  }});
 });
 const metadata=JSON.parse(await fs.readFile('web/firmware/template.json','utf8'));
 await page.route('**/vendor/fm1ota.js*',route=>route.fulfill({contentType:'text/javascript',body:`
  export class Updater {
   constructor() { this.waits=0; }
   device(text='FM-1_015') { return {name:'Simulated FM-1',id:{model:text.startsWith('ota')?'ota-FM-1':'FM-1',text,version:15},link:{send(){return true},async close(){}}}; }
   async findAny() { return this.device(); }
   async serve() { return {finished:true}; }
   async write() { window.midiWrites++;return {finished:true}; }
   async waitForAny() { return this.device(++this.waits===1?'ota-FM-1_015':'${metadata.product}'); }
  }
 `}));
 await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>!document.getElementById('dropzone').disabled);
 assert.equal(await page.locator('.crt-tv #screen').count(),1,'NES canvas belongs inside the CRT');
 assert.equal(await page.locator('.rig-logo').textContent(),'NESynth');
 assert.equal(await page.locator('.controller button[data-bit]').count(),8);
 for(const model of ['smk-pro','fm1','nes']){
  await page.locator('#simulator-model').selectOption(model);
  await page.waitForFunction(model=>document.querySelector('.console').dataset.simulator===model,model);
  assert(await page.locator('#screen').isVisible(),'Live canvas remains visible on '+model);
  if(model!=='nes')assert(await page.locator('.crt-tv').isHidden());
 }
 const tv=await page.locator('.crt-tv').boundingBox(),nes=await page.locator('.nes-console').boundingBox(),pad=await page.locator('.controller').boundingBox();
 assert(tv.y+tv.height<nes.y&&nes.y+nes.height<pad.y,'CRT, NES, controller stack vertically');
 assert.equal(await page.locator('#dropzone').count(),1);
 assert.equal(await page.locator('#step-game #rom').count(),1,'The only cartridge picker belongs to step 1');
 assert.equal(await page.locator('.cartridge-panel,#install-choose-game').count(),0,'No duplicate cartridge flow');
 const picker=page.waitForEvent('filechooser');await page.locator('#dropzone').click();
 await (await picker).setFiles('web/diagnostic.nes');
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='diagnostic.nes'&&!document.getElementById('dropzone').disabled);
 async function dropFile(file,name=path.basename(file)){
  const bytes=[...await fs.readFile(file)];
  const dataTransfer=await page.evaluateHandle(({bytes,name})=>{
   const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],name));return transfer;
  },{bytes,name});
  await page.locator('#dropzone small').dispatchEvent('dragenter',{dataTransfer});
  assert(await page.locator('#dropzone').evaluate(el=>el.classList.contains('drag-over')),'Nested drop target highlights');
  await page.locator('#dropzone small').dispatchEvent('drop',{dataTransfer});
  await page.waitForFunction(()=>!document.getElementById('dropzone').disabled);
  assert(!await page.locator('#dropzone').evaluate(el=>el.classList.contains('drag-over')));
  await dataTransfer.dispose();
 }
 await dropFile('web/diagnostic.nes','Dropped game.nes');
 assert.equal(await page.locator('#rom-name').textContent(),'Dropped game.nes');
 await dropFile('build/zip-tests/deflated.zip');
 assert.match(await page.locator('#rom-name').textContent(),/diagnostic.nes/i);
 await dropFile('build/zip-tests/multiple.zip');
 assert(await page.locator('#step-game #archive-picker').isVisible(),'ZIP choices stay in the same flow');
 assert.equal(await page.locator('#archive-entry option').count(),2);
 await page.locator('#archive-entry').selectOption('1');await page.locator('#archive-play').click();
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='Second diagnostic.nes'&&!document.getElementById('dropzone').disabled);
 assert(await page.locator('#flash-device').isVisible(),'Install action stays visible before preparation');
 assert(await page.locator('#flash-device').isDisabled());
 await page.locator('[data-install][data-homebrew="from-below"]').click();
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='From Below'&&!document.getElementById('build-firmware').disabled);
 assert.equal(await page.locator('#pause').textContent(),'Pause','Selected game also previews in the browser');
 assert.match(await page.locator('#flash-status').textContent(),/Next:.*Connect/);
 await page.evaluate(()=>window.denyMidi=true);
 await page.locator('#connect-device').click();
 await page.waitForFunction(()=>document.getElementById('flash-status').classList.contains('error'));
 assert.match(await page.locator('#flash-status').textContent(),/MIDI access was blocked/);
 assert(await page.locator('#connection-help').evaluate(el=>el.open));
 await page.evaluate(()=>window.denyMidi=false);
 await page.locator('#connect-device').click();
 await page.waitForFunction(()=>document.getElementById('device-status').textContent.includes('factory V15'));
 await page.locator('#build-firmware').click();
 await page.waitForFunction(()=>!document.getElementById('flash-device').disabled);
 assert.equal(await page.evaluate(()=>window.midiWrites),0,'Choosing, connecting and preparing must not install');
 assert.match(await page.locator('#flash-status').textContent(),/Next:.*Install game/);
 await page.locator('#flash-device').click();
 await page.waitForFunction(()=>document.getElementById('install-state').textContent==='Verified');
 assert.equal(await page.evaluate(()=>window.midiWrites),1);
 await page.locator('[data-install][data-homebrew="codemaster"]').click();
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='CodeMaster'&&!document.getElementById('build-firmware').disabled);
 assert(await page.locator('#flash-device').isDisabled(),'A different cartridge requires new preparation');
 assert.equal(await page.locator('#install-state').textContent(),'Not installed');
 await page.locator('#build-firmware').click();
 await page.waitForFunction(()=>!document.getElementById('flash-device').disabled);
 const bad=Buffer.from('Not a NES file');
 await page.locator('#rom').setInputFiles({name:'broken.nes',mimeType:'application/octet-stream',buffer:bad});
 await page.waitForFunction(()=>document.getElementById('flash-status').classList.contains('error'));
 assert(await page.locator('#flash-device').isDisabled(),'Rejected files must not leave a stale game installable');
 assert(await page.locator('#dropzone').isEnabled(),'File errors must allow another selection');
 await page.locator('#flash-model').selectOption('smk37');
 assert(await page.locator('#fm1-steps').isHidden());
 assert(await page.locator('#dropzone').isVisible(),'SMK browser users retain the same game picker');
 assert(await page.locator('#restore-device').isDisabled());
 assert.match(await page.locator('#flash-availability').textContent(),/installation is not available/);
 await page.locator('#flash-model').selectOption('fm1');
 await page.locator('[data-install][data-homebrew="from-below"]').click();
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='From Below'&&!document.getElementById('build-firmware').disabled);
 await page.locator('#connection-help').evaluate(el=>el.open=false);
 await page.locator('#install-progress').evaluate(el=>el.open=false);
 await page.evaluate(()=>window.scrollTo(0,0));
 await fs.mkdir('build/install-ui',{recursive:true});
 await page.screenshot({path:'build/install-ui/desktop.png',fullPage:true});
 await page.locator('.retro-rig').screenshot({path:'build/install-ui/retro-simulator.png'});
 const playerBox=await page.locator('.play-column').boundingBox(),installBox=await page.locator('#flash-panel').boundingBox();
 assert(installBox.x>playerBox.x&&Math.abs(installBox.y-playerBox.y)<2,'Desktop installer belongs beside the player');
 for(const width of [390,768]){
  await page.setViewportSize({width,height:900});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'No horizontal page overflow at '+width);
  await page.locator('#flash-panel').scrollIntoViewIfNeeded();
  await page.screenshot({path:`build/install-ui/mobile-${width}.png`,fullPage:true});
 }
 const ids=await page.locator('[id]').evaluateAll(nodes=>nodes.map(n=>n.id));assert.equal(ids.length,new Set(ids).size,'No duplicate IDs');
 assert.deepEqual(errors,[]);
 console.log('PASS: single game picker, click and nested NES/ZIP drops, multi-game ZIP selection, desktop/mobile layout, factory V15 connection, blocked MIDI, preparation, simulated install, stale-game rejection and SMK gating.');
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
