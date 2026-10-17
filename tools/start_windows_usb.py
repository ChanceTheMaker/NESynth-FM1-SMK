# SPDX-License-Identifier: GPL-3.0-only
"""Start the fixed-action Windows USB helper for the official NESynth site."""
from pathlib import Path
import os,subprocess,sys,shutil
ROOT=Path(__file__).resolve().parents[1]
if os.name!='nt':raise SystemExit('This helper is for Windows. Other systems use the browser installer.')
if not shutil.which('node'):raise SystemExit('Install Node.js 22 or newer, then run this helper again.')
deps=ROOT/'build/python-deps'
if not (deps/'mido').is_dir() or not (deps/'rtmidi').is_dir():
    subprocess.run([sys.executable,'-m','pip','install','--target',str(deps),'mido==1.3.3','python-rtmidi==1.5.8'],check=True)
stock=ROOT/'build/smk37/stock-pro.fwsc'
if not stock.is_file():
    subprocess.run(['node','--input-type=module','-e',"import fs from 'node:fs/promises';import {getSmkStock} from './web/smk-firmware.js';const stock=await getSmkStock('SMK-37 Pro');await fs.mkdir('build/smk37',{recursive:true});await fs.writeFile('build/smk37/stock-pro.fwsc',stock);"],cwd=ROOT,check=True)
print('Keep this helper running while installing from the NESynth website. Ctrl+C stops it.',flush=True)
raise SystemExit(subprocess.call([sys.executable,str(ROOT/'tools/serve.py'),'--port','8817','--allow-hosted'],cwd=ROOT))
