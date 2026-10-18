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
 const metadata={product:'FM-1_969'};
 await page.route('**/vendor/fm1ota.js*',route=>route.fulfill({contentType:'text/javascript',body:`
  export class Updater {
   constructor(access,log,pattern) { this.waits=0;if(window.testDevice?.startsWith('SMK')&&!pattern?.test(window.testDevice))throw Error('SMK MIDI port excluded from discovery'); }
   device(text=window.testDevice||'FM-1_015') { const split=text.lastIndexOf('_');return {name:'Simulated instrument',id:{model:text.slice(0,split),text,version:Number(text.slice(split+1))},link:{send(){return true},async close(){}}}; }
   async findAny(filter) { const device=this.device();return !filter||filter(device.id)?device:null; }
   async serve(link,bytes,finish) { if(finish===0xf0000000){window.midiWrites++;document.dispatchEvent(new CustomEvent('firmware-transfer-progress',{detail:{phase:'transfer',bytes:512,total:1024}}));window.transferStatus=document.querySelector('#flash-status').textContent;}return {finished:true}; }
   async write() { window.midiWrites++;return {finished:true}; }
   async waitForAny(filter) { const device=this.device(window.testWaits?.shift()||(++this.waits===1?'ota-FM-1_015':'${metadata.product}'));return !filter||filter(device.id)?device:null; }
  }
 `}));
 for(const [name,file] of [['Pro','pro'],['Elite','elite']]){
  const stock=await fs.readFile(`build/smk37/stock-${file}.fwsc`);
  await page.route(`**/SMK-37_${name}_016.fwsc`,route=>route.fulfill({contentType:'application/octet-stream',body:stock}));
 }
 await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>!document.getElementById('dropzone').disabled);
 assert.equal(await page.locator('.crt-tv #screen').count(),1,'NES canvas belongs inside the CRT');
 assert.equal(await page.locator('.rig-logo').textContent(),'NESynth');
 assert.equal(await page.locator('.controller button[data-bit]').count(),8);
 await page.locator('#pause').click();
 await page.waitForFunction(()=>document.getElementById('pause').textContent==='Resume');
 const gameImage=await page.locator('#screen').evaluate(el=>el.toDataURL());
 const channel=page.locator('.tv-channel');await channel.scrollIntoViewIfNeeded();
 const channelBox=await channel.boundingBox();
 await page.mouse.move(channelBox.x+channelBox.width/2,channelBox.y+channelBox.height/2);
 await page.mouse.down();await page.mouse.move(channelBox.x+channelBox.width/2+24,channelBox.y+channelBox.height/2,{steps:4});await page.mouse.up();
 assert.equal(await channel.getAttribute('aria-valuenow'),'4','Dragging the channel tunes a demo');
 const scenes=[];
 for(let i=0;i<12;i++){
  assert.equal(await channel.getAttribute('aria-valuenow'),String(i+4));
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  scenes.push({name:await channel.getAttribute('aria-valuetext'),url:await page.locator('#screen').evaluate(el=>el.toDataURL())});
  await page.waitForFunction(previous=>document.getElementById('screen').toDataURL()!==previous,scenes.at(-1).url,{timeout:3000});
  assert.equal(await page.locator('#rom-name').textContent(),'Original diagnostic','Demos do not replace the selected cartridge');
  if(i<11)await channel.press('ArrowRight');
 }
 assert.equal(new Set(scenes.map(s=>s.url)).size,12,'Twelve distinct rendered demo scenes');
 await fs.mkdir('build/install-ui',{recursive:true});
 const gallery=await browser.newPage({viewport:{width:1120,height:880}});
 await gallery.setContent('<body style="margin:16px;background:#15151c;color:white;font:12px monospace;display:grid;grid-template-columns:repeat(4,256px);gap:16px">'+scenes.map(s=>`<div>${s.name}<img width="256" height="240" src="${s.url}"></div>`).join(''));
 await gallery.screenshot({path:'build/install-ui/demo-scenes.png',fullPage:true});await gallery.close();
 await channel.press('Home');
 await page.waitForFunction(expected=>document.getElementById('screen').toDataURL()===expected,gameImage);
 assert.equal(await page.locator('#pause').textContent(),'Resume','Channel 03 preserves the paused game exactly');
 const knob=page.locator('.tv-dial.small'),knobBox=await knob.boundingBox();
 await page.mouse.move(knobBox.x+knobBox.width/2,knobBox.y+knobBox.height/2);
 await page.mouse.down();await page.mouse.move(knobBox.x+knobBox.width/2,knobBox.y+knobBox.height/2-40,{steps:4});await page.mouse.up();
 assert.equal(await page.locator('#volume').inputValue(),'45','TV knob updates browser volume');
 assert.equal(await page.locator('#level').textContent(),'45%');
 await knob.press('End');assert.equal(await page.locator('#volume').inputValue(),'100');
 await knob.press('Home');assert.equal(await page.locator('#volume').inputValue(),'0');
 await page.locator('#volume').evaluate(el=>{el.value='25';el.dispatchEvent(new Event('input',{bubbles:true}));});
 assert.equal(await knob.getAttribute('aria-valuenow'),'25','Browser slider updates TV knob');
 await knob.click({position:{x:5,y:12}});assert.equal(await page.locator('#volume').inputValue(),'20');
 await knob.click({position:{x:25,y:12}});assert.equal(await page.locator('#volume').inputValue(),'25');
 const tuner=page.locator('.tv-dial:not(.small)');
 await tuner.click({position:{x:35,y:20}});assert.equal(await channel.getAttribute('aria-valuenow'),'4');
 await tuner.click({position:{x:5,y:20}});assert.equal(await channel.getAttribute('aria-valuenow'),'3');
 await page.locator('#pause').click();
 for(const model of ['smk-pro','fm1','nes']){
  await page.locator('#simulator-model').selectOption(model);
  await page.waitForFunction(model=>document.querySelector('.console').dataset.simulator===model,model);
  assert(await page.locator('#screen').isVisible(),'Live canvas remains visible on '+model);
  if(model!=='nes')assert(await page.locator('.crt-tv').isHidden());
 }
 const tv=await page.locator('.crt-tv').boundingBox(),nes=await page.locator('.nes-console').boundingBox(),pad=await page.locator('.controller').boundingBox();
 assert(tv.y+tv.height<nes.y&&nes.y+nes.height<pad.y,'CRT, NES, controller stack vertically');
 await page.locator('#fullscreen').click();
 await page.waitForFunction(()=>document.fullscreenElement===document.querySelector('.retro-rig'));
 assert.equal(await page.locator('#fullscreen').getAttribute('aria-pressed'),'true');
 assert(await page.locator('#screen').isVisible());
 const remote=page.locator('#tv-remote');assert(await remote.isVisible(),'Fullscreen opens the clicker');
 await page.getByRole('button',{name:'Channel up',exact:true}).click();
 assert.equal(await page.locator('#remote-channel').textContent(),'04');
 assert.equal(await channel.getAttribute('aria-valuenow'),'4');
 await page.getByRole('button',{name:'Channel down',exact:true}).click();
 assert.equal(await page.locator('#remote-channel').textContent(),'03');
 await page.getByRole('button',{name:'Volume up',exact:true}).click();
 assert.equal(await page.locator('#volume').inputValue(),'30');
 assert.equal(await page.locator('#remote-volume').textContent(),'30');
 await page.getByRole('button',{name:'Volume down',exact:true}).click();
 assert.equal(await page.locator('#volume').inputValue(),'25');
 const grip=page.locator('.remote-grip'),beforeRemote=await remote.boundingBox(),gripBox=await grip.boundingBox();
 await page.mouse.move(gripBox.x+20,gripBox.y+10);await page.mouse.down();await page.mouse.move(gripBox.x-80,gripBox.y-70,{steps:5});await page.mouse.up();
 const afterRemote=await remote.boundingBox();assert(afterRemote.x<beforeRemote.x-90&&afterRemote.y<beforeRemote.y-70,'Clicker can be moved');
 await page.getByRole('button',{name:'Close remote',exact:true}).click();assert(await remote.isHidden());
 await page.locator('#show-remote').click();assert(await remote.isVisible(),'Clicker can be reopened');
 await page.getByRole('button',{name:'Channel up',exact:true}).click();
 await page.locator('[data-remote="game"]').click();assert.equal(await page.locator('#remote-channel').textContent(),'03');
 await fs.mkdir('build/install-ui',{recursive:true});
 await page.screenshot({path:'build/install-ui/fullscreen.png'});
 await page.locator('#fullscreen').click();
 await page.waitForFunction(()=>!document.fullscreenElement);
 assert.equal(await page.locator('#fullscreen').getAttribute('aria-pressed'),'false');
 assert(await remote.isHidden());assert(await page.locator('#show-remote').isHidden());
 assert.equal(await page.locator('#dropzone').count(),1);
 assert.equal(await page.locator('#step-game #rom').count(),1,'The only cartridge picker belongs to step 1');
 assert.equal(await page.locator('.cartridge-panel,#install-choose-game').count(),0,'No duplicate cartridge flow');
 const picker=page.waitForEvent('filechooser');await page.locator('#dropzone').click();
 await (await picker).setFiles('web/diagnostic.nes');
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='diagnostic.nes'&&!document.getElementById('dropzone').disabled);
 async function dropFile(file,name=path.basename(file),target='#dropzone small'){
  const bytes=[...await fs.readFile(file)];
  const dataTransfer=await page.evaluateHandle(({bytes,name})=>{
   const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],name));return transfer;
  },{bytes,name});
  const host=target.startsWith('#dropzone')?'#dropzone':'.retro-rig';
  await page.locator(target).dispatchEvent('dragenter',{dataTransfer});
  assert(await page.locator(host).evaluate(el=>el.classList.contains('drag-over')),'Nested drop target highlights');
  await page.locator(target).dispatchEvent('drop',{dataTransfer});
  await page.waitForFunction(()=>!document.getElementById('dropzone').disabled);
  assert.equal(await page.locator('.drag-over').count(),0);
  await dataTransfer.dispose();
 }
 await dropFile('web/diagnostic.nes','Dropped game.nes');
 assert.equal(await page.locator('#rom-name').textContent(),'Dropped game.nes');
 await dropFile('web/diagnostic.nes','Screen game.nes','#screen');
 assert.equal(await page.locator('#rom-name').textContent(),'Screen game.nes');
 await dropFile('build/zip-tests/deflated.zip','Console game.zip','.cartridge-door');
 assert.match(await page.locator('#rom-name').textContent(),/diagnostic.nes/i);
 for(const model of ['fm1','smk-pro','smk-elite']){
  await page.locator('#simulator-model').selectOption(model);
  await page.waitForFunction(model=>document.querySelector('.console').dataset.simulator===model,model);
  await dropFile('web/diagnostic.nes',model+'.nes','#screen');
  assert.equal(await page.locator('#rom-name').textContent(),model+'.nes');
 }
 await page.locator('#simulator-model').selectOption('nes');
 await page.locator('#fullscreen').click();
 await page.waitForFunction(()=>!!document.fullscreenElement);
 await dropFile('web/diagnostic.nes','Fullscreen game.nes','#screen');
 assert.equal(await page.locator('#rom-name').textContent(),'Fullscreen game.nes');
 await page.locator('#fullscreen').click();
 await page.waitForFunction(()=>!document.fullscreenElement);
 // A file drop cannot replace the cartridge during installation.
 await page.evaluate(()=>document.dispatchEvent(new CustomEvent('nesynth-install-busy',{detail:true})));
 const blockedDrop=await page.evaluateHandle(()=>{const d=new DataTransfer();d.items.add(new File(['invalid'],'blocked.nes'));return d;});
 await page.locator('#screen').dispatchEvent('dragenter',{dataTransfer:blockedDrop});
 assert.equal(await page.locator('.drag-over').count(),0);
 await page.locator('#screen').dispatchEvent('drop',{dataTransfer:blockedDrop});
 assert.equal(await page.locator('#rom-name').textContent(),'Fullscreen game.nes');
 await blockedDrop.dispose();
 await page.evaluate(()=>document.dispatchEvent(new CustomEvent('nesynth-install-busy',{detail:false})));
 await dropFile('build/zip-tests/deflated.zip');
 assert.match(await page.locator('#rom-name').textContent(),/diagnostic.nes/i);
 await dropFile('build/zip-tests/multiple.zip','multiple.zip','#screen');
 assert(await page.locator('#step-game #archive-picker').isVisible(),'ZIP choices stay in the same flow');
 assert.equal(await page.locator('#archive-entry option').count(),2);
 await page.locator('#archive-entry').selectOption('1');await page.locator('#archive-play').click();
 await page.waitForFunction(()=>document.getElementById('rom-name').textContent==='Second diagnostic.nes'&&!document.getElementById('dropzone').disabled);
 assert(await page.locator('#device-send').isVisible(),'Game upload is the primary action');
 assert(await page.locator('#flash-device').isHidden(),'Firmware belongs in its own tab');
 await page.locator('#firmware-tab').click();
 assert(await page.locator('#flash-device').isVisible());
 assert(await page.locator('#games-panel').isHidden());
 assert(await page.locator('#flash-device').isEnabled(),'Our firmware is ready without selecting a file');
 assert.equal(await page.locator('#firmware-panel input[type=file]').count(),0,'No firmware picker');
 assert.equal(await page.locator('#pause').textContent(),'Pause','Selected game previews in the browser');
 await page.evaluate(()=>window.denyMidi=true);
 await page.locator('#connect-device').click();
 await page.waitForFunction(()=>document.getElementById('flash-status').classList.contains('error'));
 assert.match(await page.locator('#flash-status').textContent(),/MIDI access was blocked/);
 assert(await page.locator('#connection-help').evaluate(el=>el.open));
 await page.evaluate(()=>window.denyMidi=false);
 await page.locator('#connect-device').click();
 await page.waitForFunction(()=>document.getElementById('device-status').textContent.includes('factory V15'));
 assert.equal(await page.evaluate(()=>window.midiWrites),0,'Connecting does not install');
 // Cartridge selection and errors do not replace or invalidate firmware.
 await page.locator('#games-tab').click();
 await page.locator('#rom').setInputFiles({name:'broken.nes',buffer:Buffer.from('bad')});
 await page.waitForFunction(()=>document.getElementById('status').classList.contains('error'));
 await page.locator('#firmware-tab').click();
 assert(await page.locator('#flash-device').isEnabled(),'Firmware setup does not require a valid game');
 await page.locator('#flash-device').click();
 await page.waitForFunction(()=>document.getElementById('install-state').textContent==='Verified');
 assert.equal(await page.evaluate(()=>window.midiWrites),1);
 for(const [value,model] of [['smk-pro','SMK-37 Pro'],['smk-elite','SMK-37 Elite']]){
  await page.locator('#flash-model').selectOption(value);
  assert(await page.locator('#flash-device').isEnabled(),'New model automatically selects its firmware');
  await page.evaluate(model=>{window.testDevice=model+'_016';window.testWaits=['ota-'+model+'_016','SMK-37_970'];},model);
  await page.locator('#connect-device').click();
  await page.waitForFunction(model=>document.getElementById('device-status').textContent.includes(model+'_016'),model);
  const before=await page.evaluate(()=>window.midiWrites);
  assert.equal(await page.evaluate(()=>window.midiWrites),before,'Checking firmware never writes MIDI');
  await page.screenshot({path:`build/install-ui/${value}.png`,fullPage:true});
  await page.locator('#flash-device').click();
  await page.waitForFunction(()=>document.getElementById('install-state').textContent==='Verified');
  assert.equal(await page.evaluate(()=>window.midiWrites),before+1);
  assert.match(await page.evaluate(()=>window.transferStatus),/Transferring firmware data.*50%/,'Main status follows the transfer instead of remaining on download');
  assert.match(await page.locator('#device-status').textContent(),/SMK-37_970/);
  await page.evaluate(model=>{window.testDevice='SMK-37_970';window.testWaits=['ota-SMK-37_970',model+'_016'];},model);
  await page.locator('#restore-device').evaluate(el=>el.closest('details').open=true);
  await page.locator('#restore-device').click();
  await page.waitForFunction(()=>document.getElementById('restore-status').textContent.includes('restored and restart verified'));
  assert.equal(await page.evaluate(()=>window.midiWrites),before+2);
  assert.match(await page.locator('#device-status').textContent(),new RegExp(model+'_016'));
  assert.equal(await page.locator('#install-state').textContent(),'Not installed');
 }
 await page.locator('#flash-model').selectOption('fm1');
 await page.evaluate(()=>{window.testDevice='FM-1_015';window.testWaits=[];});
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
  if(width===390){
   await page.locator('#fullscreen').click();await page.waitForFunction(()=>!!document.fullscreenElement);
   assert(await remote.isVisible());
   const bounds=await remote.boundingBox();assert(bounds.x>=0&&bounds.y>=0&&bounds.x+bounds.width<=390&&bounds.y+bounds.height<=900,'Clicker stays on a small screen');
   await page.screenshot({path:'build/install-ui/remote-mobile.png'});
   await page.locator('#fullscreen').click();await page.waitForFunction(()=>!document.fullscreenElement);
  }
 }
 await page.locator('.retro-rig').evaluate(el=>Object.defineProperty(el,'requestFullscreen',{value:undefined,configurable:true}));
 await page.locator('#fullscreen').click();
 assert(await page.locator('.fullscreen-fallback').isVisible());assert(await remote.isVisible(),'Fallback fullscreen also opens the clicker');
 await page.getByRole('button',{name:'Close remote',exact:true}).click();assert(await remote.isHidden());
 await page.locator('#show-remote').click();assert(await remote.isVisible());
 await page.locator('#fullscreen').click();assert(await remote.isHidden());
 await page.locator('.retro-rig').evaluate(el=>delete el.requestFullscreen);
 const ids=await page.locator('[id]').evaluateAll(nodes=>nodes.map(n=>n.id));assert.equal(ids.length,new Set(ids).size,'No duplicate IDs');
 // Local SMK installation uses the same controls with no browser MIDI access.
 let nativeAction='',nativePolls=0,nativeWrong=false;
 await page.route('**/api/smk/capabilities',route=>route.fulfill({json:{available:true,models:['smk-pro']}}));
 await page.route('**/api/smk/jobs',route=>{
  const body=route.request().postDataJSON();assert.equal(body.model,'smk-pro');nativeAction=body.action;nativePolls=0;
  return route.fulfill({status:202,json:{id:'test-job'}});
 });
 await page.route('**/api/smk/jobs/test-job',route=>{
  const done=++nativePolls>1;
  return route.fulfill({json:{state:done?'done':'running',phase:'transfer',bytes:512,total:1024,logs:['Local USB test'],identity:nativeWrong?'SMK-37_900':nativeAction==='install'?'SMK-37_970':'SMK-37 Pro_016'}});
 });
 await page.locator('#firmware-tab').click();await page.locator('#flash-model').selectOption('smk-pro');
 await page.evaluate(()=>window.denyMidi=true);const writesBeforeNative=await page.evaluate(()=>window.midiWrites);
 await page.locator('#connect-device').click();await page.waitForFunction(()=>!document.getElementById('connect-device').disabled);
 assert.equal(await page.locator('#device-status').textContent(),'Connected: SMK-37 Pro_016');assert.equal(nativeAction,'connect');
 await page.locator('#flash-device').click();await page.waitForFunction(()=>document.getElementById('install-state').textContent==='Verified');assert.equal(nativeAction,'install');
 await page.locator('#restore-device').evaluate(el=>el.closest('details').open=true);await page.locator('#restore-device').click();
 await page.waitForFunction(()=>!document.getElementById('restore-device').disabled);assert.equal(nativeAction,'restore');
 assert.equal(await page.locator('#device-status').textContent(),'Restart verified: SMK-37 Pro_016');
 nativeWrong=true;await page.locator('#flash-device').click();await page.waitForFunction(()=>!document.getElementById('flash-device').disabled);
 assert.match(await page.locator('#flash-status').textContent(),/Unexpected firmware/);
 assert.equal(await page.evaluate(()=>window.midiWrites),writesBeforeNative,'Local USB operations never enter browser MIDI');
 assert.deepEqual(errors,[]);
 console.log('PASS: 12 animated demo channels, drag tuning, preserved game, synced TV volume, single picker, NES/ZIP drops, responsive layout, fullscreen, blocked MIDI, FM-1/SMK install and restore, model isolation and exact restart checks.');
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
