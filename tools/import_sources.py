"""One-time pinned source import. Does not modify sibling projects."""
import concurrent.futures, hashlib, json, shutil, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REF = ROOT.parent / 'arcade-fm1-smk'
REV = 'fe3295c0a86bf5bbecc22aeab3b3af11f6f47908'
FILES = ['InfoNES.cpp','InfoNES.h','InfoNES_System.h','InfoNES_Types.h',
         'InfoNES_pAPU.cpp','InfoNES_pAPU.h','K6502.cpp','K6502.h','K6502_rw.h',
         'InfoNES_Mapper.h','mapper/InfoNES_Mapper_000.cpp']

def fetch(name):
    upstream = 'LICENSE' if name == 'LICENSE' else 'src/' + name
    url = f'https://raw.githubusercontent.com/jay-kumogata/InfoNES/{REV}/{upstream}'
    data = urllib.request.urlopen(url, timeout=60).read()
    dest = ROOT / 'vendor/infones' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return {'path':str(dest.relative_to(ROOT)).replace('\\','/'),
            'upstream':url,'sha256':hashlib.sha256(data).hexdigest()}

def main():
    if (ROOT/'src/nes_port.cpp').exists():
        raise SystemExit('Bootstrap already completed; refusing to overwrite the port. Normal builds use tools/build.py.')
    with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:
        imported = list(pool.map(fetch, FILES + ['LICENSE']))
    shutil.copytree(REF/'vendor/felucca', ROOT/'vendor/felucca', dirs_exist_ok=True)
    paths = ['firmware/board.c','firmware/start.S','firmware/multicore_video.h',
             'firmware/master_volume.h','firmware/fm1_recovery.h','firmware/smk37_game_pads.h',
             'src/smk37_game_controls.h','src/fm1_game_lights.h','src/libc.c',
             'src/freestanding/string.h','src/freestanding/stdlib.h','LICENSE']
    for name in paths:
        dest = ROOT/name
        dest.parent.mkdir(parents=True, exist_ok=True)
        data = (REF/name).read_bytes()
        dest.write_bytes(data)
        imported.append({'path':name,'upstream':f'../arcade-fm1-smk/{name}',
                         'sha256':hashlib.sha256(data).hexdigest()})
    for f in sorted((ROOT/'vendor/felucca').rglob('*')):
        if f.is_file():
            imported.append({'path':f.relative_to(ROOT).as_posix(),
                'upstream':'../arcade-fm1-smk/'+f.relative_to(ROOT).as_posix(),
                'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
    (ROOT/'vendor/provenance.json').write_text(json.dumps({
        'infones_revision':REV,'files':imported},indent=2)+'\n')
    print('Imported pinned InfoNES core and local hardware platform.')

if __name__ == '__main__':
    main()
