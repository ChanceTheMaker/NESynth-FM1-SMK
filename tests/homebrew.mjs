import fs from 'node:fs/promises';
import assert from 'node:assert/strict';
import {games,fetchHomebrew} from '../web/homebrew.js';
import {prepareFirmware} from '../web/rom-firmware.js';
import {Package} from '../web/vendor/fm1pkg.js';
const read=async p=>new Uint8Array(await fs.readFile(new URL('../web/'+p,import.meta.url)));
const fetcher=async p=>new Response(await read(p));
const template=await read('firmware/fm1-template.fwsc');
const metadata=JSON.parse(await fs.readFile('web/firmware/template.json','utf8'));
for(const entry of games){
    const {game,bytes}=await fetchHomebrew(entry.id,fetcher);
    assert.equal(game,entry);
    await read(entry.license_file);await read(entry.screenshot);
    if(entry.source_download)await read(entry.source_download);
    const g=(await WebAssembly.instantiate(await read('nes.wasm'),{})).instance.exports;
    new Uint8Array(g.memory.buffer,g.gp_upload(),bytes.length).set(bytes);
    assert.equal(g.gp_load_upload(bytes.length),0);
    for(let frame=0;frame<3600;frame++){
        g.gp_buttons([400,900,1600,2200].some(n=>frame>=n&&frame<n+10)?8:frame>2500?16:0);
        assert.equal(g.gp_frame(),1);assert.equal(g.gp_error(),0);
        g.gp_render_audio(g.gp_audio_queued());
    }
    const built=await prepareFirmware(bytes,template,metadata),pkg=new Package(built.bytes);
    const start=pkg.appOff+metadata.cartridge_offset;
    assert.deepEqual(pkg.dec.subarray(start,start+bytes.length),bytes);
    console.log('PASS:',entry.title,'— verified download, 3600 frames, exact firmware cartridge bytes');
}
await assert.rejects(fetchHomebrew('unknown',fetcher),/Unknown/);
await assert.rejects(fetchHomebrew(games[0].id,async()=>new Response('',{status:404})),/download failed/);
await assert.rejects(fetchHomebrew(games[0].id,async()=>new Response(new Uint8Array(16))),/incomplete or changed/);
console.log('PASS: unknown, missing and corrupted homebrew downloads rejected.');
