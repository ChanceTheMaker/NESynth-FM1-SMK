"""Capture-derived previews; run tools/smoke_rom.mjs for each bundled game first."""
from pathlib import Path
import base64
import hashlib
import json
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'web'
games = [
    dict(id='from-below', title='From Below', author='Matt Hughson', genre='Falling blocks',
         description='Stack blocks. Clear lines. Keep the kraken at bay.',
         controls='Press Start through the intro and menus. Left/right move; A/B rotate; down drops.',
         license='MIT', source='https://github.com/mhughson/mbh-firstnes',
         revision='8a8bfcf7e0b395350fd97b3e40430ba8182fcf51',
         upstream_path='promotion/itch_emulator/roms/from_below_2020_09_16_v_1_0_0.nes'),
    dict(id='codemaster', title='CodeMaster', author='Daniel Vukelich', genre='Code-breaking puzzle',
         description='Four symbols. Ten guesses. Crack the secret code.',
         controls='Start selects one player and difficulty. Left/right choose a slot; up/down change symbols; A submits. This game has no soundtrack.',
         license='GPL-3.0', source='https://github.com/DanielVukelich/Codemaster-NES-homebrew',
         revision='db5405b2860af7f6238dca13f398c505f4a4529d', upstream_path='Game/ROM/CodeMaster_Main.nes',
         source_download='homebrew/codemaster-source.zip'),
    dict(id='diamond-chase', title='Diamond Chase', author='TeneoPython01', genre='Mini arcade',
         description='A tiny chase with a chiptune reward. Find the yellow target.',
         controls='Use the direction keys to guide the white diamond into the yellow target. Start plays again after a win.',
         license='MIT', source='https://github.com/TeneoPython01/nes-game-01',
         revision='b7ee8416d33b2eb52f4b346f6385a5826cf55cff', upstream_path='game.nes'),
]
for game in games:
    slug = game['id']
    rom = WEB / 'homebrew' / f'{slug}.nes'
    game.update(rom=f'homebrew/{slug}.nes', screenshot=f'homebrew/{slug}.png',
                license_file=f'homebrew/{slug}-LICENSE.txt', sha256=hashlib.sha256(rom.read_bytes()).hexdigest())
    Image.open(ROOT / f'build/homebrew-smoke/{slug}/frame-3600.ppm').save(WEB / game['screenshot'])
(WEB / 'homebrew/catalog.js').write_text('export const games = ' + json.dumps(games, indent=2) + ';\n', encoding='utf-8')

# Retain the detailed native vector artwork and insert genuine emulator frames.
for model, slug, rect in [('fm1','from-below',(200,95,103,93)), ('smk-pro','codemaster',(137,80,72,65))]:
    svg = (WEB / f'artwork/{model}.svg').read_text(encoding='utf-8')
    data = base64.b64encode((WEB / f'homebrew/{slug}.png').read_bytes()).decode()
    x,y,w,h = rect
    screen = f'<image href="data:image/png;base64,{data}" x="{x}" y="{y}" width="{w}" height="{h}" preserveAspectRatio="none" style="image-rendering:pixelated"/>'
    svg = svg.replace('</svg>', screen + '</svg>').replace('NESynth simulator', 'NESynth illustration with homebrew gameplay')
    (WEB / f'artwork/hero-{model}.svg').write_text(svg, encoding='utf-8')
print('Wrote three real gameplay previews, catalog, and two instrument illustrations.')
