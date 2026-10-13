"""Build the browser firmware template using only our original NES diagnostic."""
import hashlib,json,shutil,subprocess,sys
from pathlib import Path
from diagnostic import make_rom
ROOT=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,'tools/build.py','fm1','--rom-capacity','393744'],cwd=ROOT,check=True)
subprocess.run([sys.executable,'tools/package_fm1.py','--label','template'],cwd=ROOT,check=True)
m=json.loads((ROOT/'build/fm1/manifest.json').read_text())
p=json.loads((ROOT/'build/fm1/trial-template.json').read_text())
app=(ROOT/'build/fm1/nes-NOT-FOR-INSTALL.bin').read_bytes()
offset=m['cartridge_offset'];capacity=m['cartridge_capacity'];diag=make_rom()
assert m['usb_updater'] is True and m['dual_core'] is True
assert app[offset:offset+len(diag)]==diag
assert app[offset+len(diag):offset+capacity]==b'\xff'*(capacity-len(diag))
out=ROOT/'web/firmware';out.mkdir(parents=True,exist_ok=True)
shutil.copy2(ROOT/'build/fm1'/p['package'],out/'fm1-template.fwsc')
subprocess.run([str(ROOT/'build/fm1/ldr-test.exe'),str(out/'fm1-template.fwsc'),str(out/'fm1-original.fwsc')],cwd=ROOT,check=True)
meta={'product':p['product'],'package':'fm1-template.fwsc','sha256':p['sha256'],
      'cartridge_offset':offset,'cartridge_capacity':capacity,
      'usb_updater':True,'usb_protocol':'fm1-ota-v1','dual_core':True,'mappers':[0,1,9],
      'protected_app_sha256':hashlib.sha256(app[:offset]+app[offset+capacity:]+b'\xff'*(0x8dfbc-len(app))).hexdigest(),
      'loader_sha256':p['loader_sha256'],'app_bytes':len(app),
      'diagnostic_sha256':hashlib.sha256(diag).hexdigest(),
      'notice':'Original diagnostic only. User ROMs are inserted locally in the browser.',
      'hardware_status':'Experimental FM-1 firmware. Picture, controls and both cores tested. Adaptive drawing prioritizes game timing and audio; compatibility remains experimental. SMK37 flashing is not enabled.'}
(out/'template.json').write_text(json.dumps(meta,indent=2)+'\n')
print(json.dumps(meta,indent=2))
