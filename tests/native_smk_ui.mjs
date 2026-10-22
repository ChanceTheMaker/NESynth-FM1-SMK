// Hosted installer regression: every USB request is simulated; no hardware is accessed.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
const {chromium}=await import(process.env.PLAYWRIGHT_MODULE||'playwright');
const app='nesynth',expected='SMK-37_971';
const base='https://chancethemaker.github.io/NESynth-FM1-SMK/';
const helper='http://127.0.0.1:8817',token='t'.repeat(43);
const appHash="232364b0b828851a17881a4da729f98e3488fdf280fcea88e977364d9c0758c8";
const types={'.js':'text/javascript','.html':'text/html','.css':'text/css','.svg':'image/svg+xml','.json':'application/json','.wasm':'application/wasm'};
const browser=await chromium.launch({channel:'chrome',headless:true});
try{
 const context=await browser.newContext({permissions:['local-network-access']});
 const page=await context.newPage();let mode='ready',probes=0,actions=[],errors=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{
  if(window===window.top)window.midiCalls=0;
  Object.defineProperty(navigator,'platform',{value:'Win32'});
  Object.defineProperty(navigator,'userAgentData',{value:{platform:'Windows'}});
  Object.defineProperty(navigator,'requestMIDIAccess',{value:async()=>{window.top.midiCalls++;if(window.top.allowEmptyMidi){const port=()=>({name:'Unrelated test device',state:'connected',close:async()=>{window.top.closedMidiPorts++;}});return {sysexEnabled:true,inputs:new Map([['i',port()]]),outputs:new Map([['o',port()]])};}throw Error('Simulated browser MIDI permission');}});
 });
 await page.route(base+'**',async route=>{
  const relative=decodeURIComponent(new URL(route.request().url()).pathname.slice(new URL(base).pathname.length))||'index.html';
  try{await route.fulfill({contentType:types[path.extname(relative)]||'application/octet-stream',body:await fs.readFile(path.join('web',relative))});}
  catch{await route.fulfill({status:404,body:'Not found'});}
 });
 await page.route(helper+'/**',async route=>{
  const request=route.request(),url=new URL(request.url());
  if(url.pathname.endsWith('/capabilities')){
   probes++;
   if(mode==='absent')return route.abort('connectionrefused');
   return route.fulfill({json:{available:true,models:['smk-pro'],protocol:1,token,application_sha256:mode==='legacy'?undefined:appHash,app:mode==='wrong'?'wrong-site':app,firmware:expected}});
  }
  if(request.method()==='POST'){
   assert.equal(request.headers()['x-nesynth-token'],token);
   const body=request.postDataJSON();assert.equal(body.app,undefined);assert.equal(body.model,'smk-pro');actions.push(body.action);
   return route.fulfill({json:{id:'simulated-job'}});
  }
  return route.fulfill({json:{state:'done',phase:'reboot',logs:['Simulated USB restart'],identity:actions.at(-1)==='restore'?'SMK-37 Pro_016':expected}});
 });
 await page.goto(base);await page.locator('#firmware-tab').click();
 await page.locator('#flash-model').selectOption('smk-pro');
 assert.equal(probes,0,'No helper probe before a user action');
 assert(await page.locator('#smk-helper-help').isVisible());
 async function clickDone(selector){await page.locator(selector).click();await page.waitForFunction(()=>!document.querySelector('#connect-device').disabled);}
 for(const failure of ['absent','wrong']){
  mode=failure;await clickDone('#connect-device');
  assert.match(await page.locator('#device-status').textContent(),/browser MIDI updater was not started/);
  assert.equal(await page.evaluate(()=>window.midiCalls),0);
 }
 mode='ready';await clickDone('#connect-device');
 assert.equal(await page.locator('#device-status').textContent(),'Connected: '+expected);
 await clickDone('#flash-device');assert.equal(await page.locator('#device-status').textContent(),'Restart verified: '+expected);
 await page.locator('#restore-panel').evaluate(el=>el.open=true);
 mode='legacy';await clickDone('#restore-device');assert.equal(await page.locator('#device-status').textContent(),'Restart verified: SMK-37 Pro_016');
 assert.deepEqual(actions,['connect','install','restore']);assert.equal(await page.evaluate(()=>window.midiCalls),0);
 await page.locator('#games-tab').click();assert(await page.locator('#games-panel').isVisible());
 await page.locator('#firmware-tab').click();await page.locator('#flash-model').selectOption('fm1');
 assert(await page.locator('#smk-helper-help').isHidden());
 const before=probes;await clickDone('#connect-device');assert.equal(probes,before);assert.equal(await page.evaluate(()=>window.midiCalls),1);
 assert.match(await page.locator('#device-status').textContent(),/Simulated browser MIDI permission/);
 assert.equal(await page.locator('iframe[data-midi-session]').count(),0,'Permission failure must destroy its MIDI document');
 await page.evaluate(()=>{window.allowEmptyMidi=true;window.closedMidiPorts=0;});
 await clickDone('#connect-device');
 assert.equal(await page.locator('iframe[data-midi-session]').count(),0,'Completed browser connection must end its MIDI document');
 assert.equal(await page.evaluate(()=>window.closedMidiPorts),2,'Release both directions, including unselected MIDI ports');
 const midiBefore=await page.evaluate(()=>window.midiCalls);
 await page.locator('#flash-model').selectOption('smk-pro');
 mode='ready';await clickDone('#flash-device');mode='legacy';await clickDone('#restore-device');
 assert.equal(await page.evaluate(()=>window.midiCalls),midiBefore,'SMK remains outside Web MIDI after FM-1 use');
 assert.equal(await page.locator('#device-status').textContent(),'Restart verified: SMK-37 Pro_016');

 assert.deepEqual(errors,[]);
 console.log('PASS hosted '+app+': unavailable/wrong helper blocks MIDI; connect, install and restore use bound helper; FM-1 retains browser transport; responsive UI.');
}finally{await browser.close();}
