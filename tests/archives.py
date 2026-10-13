"""ZIP fixtures from Python's standard writer, plus command-line loading tests."""
import io
import sys
import unittest
from pathlib import Path
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED, ZIP_BZIP2

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from cartridge import read_cartridge, ROM_LIMIT
from diagnostic import make_rom

OUT = ROOT / 'build/zip-tests'
OUT.mkdir(parents=True, exist_ok=True)
ROM = make_rom()


class Stream(io.BytesIO):
    def seekable(self): return False
    def seek(self, *args): raise io.UnsupportedOperation('stream')


def fixture(name, entries, method=ZIP_DEFLATED, streamed=False):
    stream = Stream() if streamed else io.BytesIO()
    with ZipFile(stream, 'w', compression=method) as archive:
        archive.comment = b'Original NES diagnostic test fixtures'
        for path, data in entries:
            archive.writestr(path, data)
    path = OUT / name
    path.write_bytes(stream.getvalue())
    return path


for method, name in [(ZIP_STORED, 'stored'), (ZIP_DEFLATED, 'deflated')]:
    fixture(name + '.zip', [('docs/readme.txt', b'Original diagnostic'), ('games/Diagnostic.NES', ROM), ('__MACOSX/._Diagnostic.nes', b'metadata')], method)
fixture('streamed.zip', [('diagnostic.nes', ROM)], streamed=True)
fixture('multiple.zip', [('First diagnostic.nes', ROM), ('folder/Second diagnostic.nes', ROM)])
fixture('empty.zip', [('readme.txt', b'No cartridge')])
fixture('oversized.zip', [('large.nes', bytes(ROM_LIMIT + 1))])
fixture('unsupported.zip', [('diagnostic.nes', ROM)], ZIP_BZIP2)
(OUT / 'diagnostic.nes').write_bytes(ROM)


class CartridgeTests(unittest.TestCase):
    def test_raw_stored_deflated_and_streamed(self):
        for name in ['diagnostic.nes', 'stored.zip', 'deflated.zip', 'streamed.zip']:
            with self.subTest(name=name):
                self.assertEqual(read_cartridge(OUT / name), ROM)

    def test_selection(self):
        with self.assertRaisesRegex(ValueError, 'multiple cartridges'):
            read_cartridge(OUT / 'multiple.zip')
        self.assertEqual(read_cartridge(OUT / 'multiple.zip', 'folder/Second diagnostic.nes'), ROM)
        with self.assertRaisesRegex(ValueError, 'No matching'):
            read_cartridge(OUT / 'multiple.zip', 'missing.nes')

    def test_rejections(self):
        for name, error in [('empty.zip', 'No matching'), ('oversized.zip', '1 MiB'), ('unsupported.zip', 'DEFLATE')]:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, error):
                read_cartridge(OUT / name)


if __name__ == '__main__': unittest.main()
