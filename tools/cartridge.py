"""Read a raw NES cartridge or one selected entry from a local ZIP."""
from pathlib import Path
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED

ROM_LIMIT = 1024 * 1024
ZIP_LIMIT = 64 * 1024 * 1024


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
        return path.read_bytes()
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
        return data
