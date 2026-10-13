// Adapted from GameDude's bounded local ZIP reader (GPL-3.0-only).
// No archive paths are written to disk. Only the chosen cartridge is inflated.
export const ROM_LIMIT = 1024 * 1024;
export const ZIP_LIMIT = 64 * 1024 * 1024;
export function crc32(bytes) {
    let crc = 0xffffffff;
    for (const byte of bytes) {
        crc ^= byte;
        for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return (crc ^ 0xffffffff) >>> 0;
}

export async function openCartridge(file) {
    if (file.size > ZIP_LIMIT) throw Error('ZIP files must be 64 MiB or smaller.');
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (!/\.zip$/i.test(file.name) && !(bytes[0] === 80 && bytes[1] === 75)) {
        if (bytes.length > ROM_LIMIT) throw Error('Cartridge exceeds the 1 MiB upload limit.');
        return [{name: file.name, read: async () => bytes}];
    }
    return zipCartridges(bytes);
}

export function zipCartridges(bytes) {
    if (bytes.length > ZIP_LIMIT) throw Error('ZIP files must be 64 MiB or smaller.');
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const u16 = p => view.getUint16(p, true), u32 = p => view.getUint32(p, true);
    let end = -1;
    for (let p = bytes.length - 22; p >= Math.max(0, bytes.length - 65557); p--) {
        if (u32(p) === 0x06054b50 && p + 22 + u16(p + 20) === bytes.length) { end = p; break; }
    }
    if (end < 0) throw Error('Invalid or truncated ZIP directory.');
    const count = u16(end + 10), length = u32(end + 12), start = u32(end + 16);
    if (u16(end + 4) || u16(end + 6) || u16(end + 8) !== count) throw Error('Split ZIP archives are not supported.');
    if (count === 65535 || length === 0xffffffff || start === 0xffffffff) throw Error('ZIP64 archives are not supported.');
    if (start + length !== end) throw Error('Invalid ZIP directory bounds.');
    const files = [];
    let p = start;
    for (let i = 0; i < count; i++) {
        if (p + 46 > end || u32(p) !== 0x02014b50) throw Error('Invalid ZIP entry.');
        const nameLength = u16(p + 28), next = p + 46 + nameLength + u16(p + 30) + u16(p + 32);
        if (next > end) throw Error('Truncated ZIP entry.');
        const nameBytes = bytes.subarray(p + 46, p + 46 + nameLength);
        const name = new TextDecoder().decode(nameBytes);
        if (/\.nes$/i.test(name) && !name.replaceAll('\\', '/').split('/').some(part => part === '__MACOSX' || part.startsWith('._'))) {
            const entry = {name, nameBytes, flags: u16(p + 8), method: u16(p + 10), crc: u32(p + 16), compressed: u32(p + 20), size: u32(p + 24), disk: u16(p + 34), offset: u32(p + 42)};
            files.push({name, read: () => extract(entry)});
        }
        p = next;
    }
    if (p !== end) throw Error('Invalid ZIP entry count.');
    if (!files.length) throw Error('This ZIP contains no .nes cartridges.');
    return files;

    async function extract(f) {
        if (f.flags & 0x41) throw Error('Password-protected ZIP entries are not supported.');
        if (f.disk || f.offset === 0xffffffff || f.compressed === 0xffffffff || f.size === 0xffffffff) throw Error('Split or ZIP64 entries are not supported.');
        if (f.size > ROM_LIMIT) throw Error('Cartridge exceeds the 1 MiB upload limit.');
        if (f.offset + 30 > start || u32(f.offset) !== 0x04034b50) throw Error('Invalid ZIP local header.');
        if (u16(f.offset + 6) !== f.flags || u16(f.offset + 8) !== f.method) throw Error('Inconsistent ZIP headers.');
        const nameStart = f.offset + 30, nameEnd = nameStart + u16(f.offset + 26), dataStart = nameEnd + u16(f.offset + 28);
        if (dataStart + f.compressed > start || nameEnd > start) throw Error('Truncated ZIP content.');
        if (nameEnd - nameStart !== f.nameBytes.length || !f.nameBytes.every((b, i) => b === bytes[nameStart + i])) throw Error('Inconsistent ZIP filenames.');
        if (!(f.flags & 8) && (u32(f.offset + 14) !== f.crc || u32(f.offset + 18) !== f.compressed || u32(f.offset + 22) !== f.size)) throw Error('Inconsistent ZIP sizes or checksum.');
        const input = bytes.subarray(dataStart, dataStart + f.compressed);
        let raw;
        if (f.method === 0) raw = input.slice();
        else if (f.method === 8) {
            let decompressor;
            try { decompressor = new DecompressionStream('deflate-raw'); }
            catch { throw Error('This browser cannot unzip DEFLATE files. Update your browser or choose an extracted .nes file.'); }
            const reader = new Blob([input]).stream().pipeThrough(decompressor).getReader();
            raw = new Uint8Array(f.size);
            let used = 0;
            try {
                for (;;) {
                    const {done, value} = await reader.read();
                    if (done) break;
                    if (used + value.length > raw.length) throw Error('ZIP size mismatch.');
                    raw.set(value, used); used += value.length;
                }
                if (used !== raw.length) throw Error('ZIP size mismatch.');
            } catch (error) {
                await reader.cancel().catch(() => {});
                throw Error('Could not unzip cartridge: ' + error.message);
            } finally { reader.releaseLock(); }
        } else throw Error('Unsupported ZIP compression. Use a standard stored or DEFLATE ZIP.');
        if (raw.length !== f.size) throw Error('ZIP size mismatch.');
        if (crc32(raw) !== f.crc) throw Error('ZIP checksum failed. The cartridge may be damaged.');
        return raw;
    }
}
