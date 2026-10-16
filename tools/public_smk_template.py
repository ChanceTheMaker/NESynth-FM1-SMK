"""Build a calibration-free SMK template containing only the original diagnostic."""
import hashlib,json,shutil,subprocess,sys
from pathlib import Path
from diagnostic import make_rom
ROOT=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,'tools/build.py','smk37','--rom-capacity','524816'],cwd=ROOT,check=True)
folder=ROOT/'build/smk37';m=json.loads((folder/'manifest.json').read_text());app=(folder/'nes-NOT-FOR-INSTALL.bin').read_bytes()
offset,capacity=m['cartridge_offset'],m['cartridge_capacity'];diagnostic=make_rom()
assert m['usb_updater'] and m['usb_protocol']=='smk37-ram-v2'
assert app[offset:offset+len(diagnostic)]==diagnostic
assert app[offset+len(diagnostic):offset+capacity]==b'\xff'*(capacity-len(diagnostic))
out=ROOT/'web/firmware';out.mkdir(exist_ok=True)
shutil.copy2(folder/'nes-NOT-FOR-INSTALL.bin',out/'smk37-template.bin')
meta={'product':m['product'],'package':'smk37-template.bin','bytes':len(app),'sha256':hashlib.sha256(app).hexdigest(),
      'models':['SMK-37 Pro','SMK-37 Elite'],'cartridge_offset':offset,'cartridge_capacity':capacity,
      'usb_updater':True,'usb_protocol':'smk37-ram-v2','dual_core':True,'mappers':m['mappers'],
      'protected_app_sha256':hashlib.sha256(app[:offset]+app[offset+capacity:]).hexdigest(),
      'loader':m['loader'],'hardware_validated':False,
      'notice':'Experimental hardware candidate. Original diagnostic only; cartridges are inserted locally. Factory boot and calibration are preserved.'}
(out/'smk37-template.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps(meta,indent=2))
