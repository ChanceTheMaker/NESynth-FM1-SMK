import assert from 'node:assert/strict';
import fs from 'node:fs';
import {openCartridge, zipCartridges, crc32, ZIP_LIMIT} from '../web/zip.js';
const read = name => new Uint8Array(fs.readFileSync('build/zip-tests/' + name));
const rom = read('diagnostic.nes');
assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926);
for (const name of ['diagnostic.nes', 'stored.zip', 'deflated.zip', 'streamed.zip']) {
    const files = await openCartridge(new File([read(name)], name));
    assert.equal(files.length, 1);
    assert.deepEqual(await files[0].read(), rom);
}
assert.equal((await openCartridge(new File([read('deflated.zip')], 'renamed.NES'))).length, 1);
const many = zipCartridges(read('multiple.zip'));
assert.deepEqual(many.map(f => f.name), ['First diagnostic.nes', 'folder/Second diagnostic.nes']);
assert.deepEqual(await many[1].read(), rom);
assert.throws(() => zipCartridges(read('empty.zip')), /no .nes/);
await assert.rejects(zipCartridges(read('oversized.zip'))[0].read(), /1 MiB/);
await assert.rejects(zipCartridges(read('unsupported.zip'))[0].read(), /compression/);
await assert.rejects(openCartridge({size: ZIP_LIMIT + 1}), /64 MiB/);
for (const length of [0, 2, 20, 100, read('deflated.zip').length - 1]) {
    assert.throws(() => zipCartridges(read('deflated.zip').slice(0, length)), /ZIP/);
}
function mutate(fn) {
    const bytes = read('streamed.zip'), v = new DataView(bytes.buffer);
    let central = -1, end = -1;
    for (let i = 0; i <= bytes.length - 4; i++) {
        if (v.getUint32(i, true) === 0x02014b50) central = i;
        if (v.getUint32(i, true) === 0x06054b50) end = i;
    }
    fn(v, central, end); return bytes;
}
await assert.rejects(zipCartridges(mutate((v, c) => v.setUint32(c + 16, 0, true)))[0].read(), /checksum/);
await assert.rejects(zipCartridges(mutate((v, c) => v.setUint32(c + 24, 10, true)))[0].read(), /size mismatch/);
await assert.rejects(zipCartridges(mutate((v, c) => v.setUint16(c + 8, 9, true)))[0].read(), /Password/);
await assert.rejects(zipCartridges(mutate((v, c) => v.setUint32(c + 42, c, true)))[0].read(), /local header/);
await assert.rejects(zipCartridges(mutate(v => v.setUint16(8, 0, true)))[0].read(), /Inconsistent/);
assert.throws(() => zipCartridges(mutate((v, c, e) => v.setUint16(e + 4, 1, true))), /Split/);
assert.throws(() => zipCartridges(mutate((v, c, e) => v.setUint32(e + 16, 0xffffffff, true))), /ZIP64/);
// Extracted bytes must enter and run in the real core, not just resemble a ROM.
const {instance} = await WebAssembly.instantiate(fs.readFileSync('web/nes.wasm'), {});
const g = instance.exports, bytes = await zipCartridges(read('deflated.zip'))[0].read();
new Uint8Array(g.memory.buffer, g.gp_upload(), bytes.length).set(bytes);
assert.equal(g.gp_load_upload(bytes.length), 0);
for (let i = 0; i < 10; i++) { assert.equal(g.gp_frame(), 1); g.gp_render_audio(g.gp_audio_queued()); }
let hash = 2166136261;
for (const b of new Uint8Array(g.memory.buffer, g.gp_pixels(), 256 * 240)) hash = Math.imul(hash ^ b, 16777619);
assert.equal(hash >>> 0, JSON.parse(fs.readFileSync('build/host-benchmark.json')).diagnostic_frame10_fnv1a);
console.log('ZIP tests passed: stored/DEFLATE, streaming headers, selection, bounds, CRC, malformed archives, WASM playback.');
