"""Read a raw NES cartridge or one selected entry from a local ZIP."""
from pathlib import Path
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED
import hashlib

ROM_LIMIT = 1024 * 1024
ZIP_LIMIT = 64 * 1024 * 1024


def normalize_cartridge(data):
    """Repair exact legacy signatures in memory; never rewrite the source file.

    Ms. Pac-Man (Tengen) identification: mamedev/mame hash/nes.xml, mspacmnt.
    PRG SHA1 83057a9b6f6a3ea8fe59af149be4c71d9c319617,
    CHR SHA1 d96321e997d4ab1fdb4c3ad23830f4c2689f4760; NROM, horizontal.
    The checksum below identifies the entire known overdump, not a filename.
    """
    if (len(data) == 262160 and data[:16] == bytes.fromhex('4e45531a081011000000006173746572') and
        hashlib.sha256(data[16:]).hexdigest() == '346fd8061a0b09c057855536012b146256bfbe386e304ef950ad93603048933c'):
        result = bytearray(data);result[11:16] = bytes(5)
        return bytes(result)
    if len(data) < 16 or data[:4] != b'NES\x1a' or data[7:16] != b'DiskDude!':
        return data
    repair_overdump = (len(data) == 65552 and data[4:7] == bytes([2, 4, 1]) and
        hashlib.sha256(data[16:]).hexdigest() ==
        'd8fedbf98da736a5dc97a75c32c9c32ba36cf14be55522a49da44dad241a2d1d')
    result = bytearray(data[:40976] if repair_overdump else data)
    result[7:16] = bytes(9)
    if repair_overdump:
        result[5] = 1
        result[6] = 0
    return bytes(result)


def read_cartridge(path, entry=None):
    path = Path(path)
    if path.stat().st_size > ZIP_LIMIT:
        raise ValueError('ZIP files must be 64 MiB or smaller')
    with path.open('rb') as source:
        is_zip = path.suffix.lower() == '.zip' or source.read(2) == b'PK'
    if not is_zip:
        if entry:
            raise ValueError('--rom-entry requires a ZIP file')
        if path.stat().st_size > ROM_LIMIT:
            raise ValueError('Cartridge exceeds the 1 MiB limit')
        return normalize_cartridge(path.read_bytes())
    with ZipFile(path) as archive:
        files = [f for f in archive.infolist() if f.filename.lower().endswith('.nes')
                 and not any(p == '__MACOSX' or p.startswith('._') for p in f.filename.replace('\\', '/').split('/'))]
        if entry:
            files = [f for f in files if f.filename == entry]
        if not files:
            raise ValueError('No matching .nes cartridge in ZIP')
        if len(files) != 1:
            raise ValueError('ZIP contains multiple cartridges; choose one with --rom-entry: ' + ', '.join(f.filename for f in files))
        item = files[0]
        if item.flag_bits & 0x41:
            raise ValueError('Password-protected ZIP entries are not supported')
        if item.compress_type not in (ZIP_STORED, ZIP_DEFLATED):
            raise ValueError('Use a standard stored or DEFLATE ZIP')
        if item.file_size > ROM_LIMIT:
            raise ValueError('Cartridge exceeds the 1 MiB limit')
        with archive.open(item) as source:
            data = source.read(ROM_LIMIT + 1)
        if len(data) != item.file_size:
            raise ValueError('ZIP size mismatch')
        return normalize_cartridge(data)
