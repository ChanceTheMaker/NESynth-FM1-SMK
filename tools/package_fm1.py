"""Build and verify a private FM-1 trial package. Never accesses a device."""
import argparse,hashlib,json,re,struct,subprocess,sys
from pathlib import Path
from build import TC as DEFAULT_TC,ZIG as DEFAULT_ZIG,ROOT
sys.path.insert(0,str(ROOT/'vendor/felucca/tools'))
import fm1pkg_make as pkg
import fm1_install as wire
import lz4blk

SDK_HASHES={
 'uboot.boot':'4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4',
 'cfg_tool.bin':'276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b',
 'cfg/eq_cfg_hw.bin':'41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d'}
def sha(b):return hashlib.sha256(b).hexdigest()
def run(args):return subprocess.check_output(list(map(str,args)),cwd=ROOT,text=True)
def wrap(image):
 def head(crc,a,b,attr):
  body=struct.pack('<HIIBBH16s',crc,a,b,attr,0,0,b'usb_hid_ota.bin')
  return struct.pack('<H',pkg.crc16(body))+body
 blocks=bytearray()
 for i in range(0,len(image),4096):
  raw=image[i:i+4096];compressed=lz4blk.compress(raw)
  assert len(compressed)<=4096 and lz4blk.decompress(compressed)==raw
  blocks+=struct.pack('<II',len(compressed),len(raw))+compressed
 body=head(pkg.crc16(image),len(image),0x01c0a800,0)+blocks
 return head(pkg.crc16(body),0x20,len(body),0x41)+body
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--sdk',type=Path,default=ROOT.parent/'Felucca/Felucca-review/.build-deps/ac79-sdk')
 p.add_argument('--bluetooth-loader',action='store_true',help='Package the separate FM-1 Bluetooth/library build')
 p.add_argument('--label',default='diagnostic');p.add_argument('--toolchain',default=DEFAULT_TC);p.add_argument('--zig',default=str(DEFAULT_ZIG));a=p.parse_args()
 for rel,digest in SDK_HASHES.items():
  assert sha((a.sdk/'cpu/wl82/tools'/rel).read_bytes())==digest,rel+' SDK revision mismatch'
 folder='build/fm1-bluetooth-loader' if a.bluetooth_loader else 'build/fm1'
 out=ROOT/folder;out.mkdir(parents=True,exist_ok=True)
 def tc(tool,*args):return run(['wsl','-e',a.toolchain+'/'+tool,*args])
 fw='vendor/felucca/firmware'
 flags=['-target','pi32v2','-Os','-ffunction-sections','-fno-builtin','-Wall','-Wno-unused-function','-I'+fw+'/hal','-I'+fw+'/src']
 for stem in ['crt0_ldr','loader']:
  tc('pi32v2/bin/clang',*flags,'-c',fw+'/loader/'+stem+('.S' if stem=='crt0_ldr' else '.c'),'-o',folder+'/'+stem+'.o')
 tc('pi32v2/bin/ld','--gc-sections','-e','_start','-T',fw+'/loader/loader.ld',folder+'/crt0_ldr.o',folder+'/loader.o','-o',folder+'/loader.elf')
 sections=tc('common/bin/objdump','-h',folder+'/loader.elf')
 for line in sections.splitlines():
  parts=line.split()
  if len(parts)>4 and parts[1].startswith('.') and int(parts[2],16):assert 0x1c00000<=int(parts[3],16)<0x1c80000,line
 tc('common/bin/objcopy','-O','binary','-j','.text',folder+'/loader.elf',folder+'/loader.bin')
 loader=wrap((out/'loader.bin').read_bytes());assert len(loader)<=0x4f00
 (out/'ota.bin').write_bytes(loader)
 app=(out/('FM1969-BLUETOOTH-RAW.bin' if a.bluetooth_loader else 'nes-NOT-FOR-INSTALL.bin')).read_bytes()
 manifest=json.loads((out/('report.json' if a.bluetooth_loader else 'manifest.json')).read_text());assert manifest['sha256']==sha(app)
 product=manifest['product'] if a.bluetooth_loader else re.search(r'#define FELUCCA_ID "(FM-1_\d+)"',(ROOT/'firmware/fm1_recovery.h').read_text())[1]
 if a.bluetooth_loader:
  assert manifest['board']=='fm1' and manifest['ram_reach_check']
  assert manifest['cartridge_flash_hi']==0x92000 and len(app)+0x4120<=manifest['cartridge_flash_lo']
 assert manifest['usb_updater'] and product.encode() in app
 assert manifest.get('dual_core') is True
 pkg.SDK=a.sdk;flash=pkg.flash_image(app,pkg.KEY);raw=pkg.ufw(flash,loader,product)
 assert wire.product_of(raw)==product
 logical=wire.logical_image(raw)
 assert logical[0x400:0x400+len(flash)]==flash
 # Recover the embedded app independently from the encrypted flash region.
 area=bytearray(flash[0x4000:]);pkg.sfc(area,0,len(area),0,pkg.KEY)
 assert area[0x120:0x120+len(app)]==app
 output=out/('nesynth-fm1-'+product.split('_')[1]+'-'+a.label+'.fwsc');output.write_bytes(raw)
 restore_source=ROOT.parent/'MusicCoach/MusicCoach-firmware/build/musiccoach-0.3-dc1/musiccoach-fm1-0.3-dc1.fwsc'
 assert sha(restore_source.read_bytes())=='4b30d50ea70a225edd5c6c44f710c75ac7f9f861e5c0989f98e40cdad009d6f8','Restore package changed'
 restore=out/'recovery-musiccoach-fm1-903.fwsc';restore.write_bytes(restore_source.read_bytes())
 assert wire.product_of(restore.read_bytes())=='FM-1_903'
 logs=[]
 for name in ['ota','ldr']:
  exe=out/(name+'-test.exe')
  run([a.zig,'cc','-O1','-UNDEBUG','vendor/felucca/tests/'+name+'_test.c','-o',exe])
  result=run([exe,output] if name=='ota' else [exe,restore,output]);logs.append(result);print(result)
  if name=='ldr':
   result=run([exe,output,restore]);logs.append(result);print(result)
 (out/'updater-tests.txt').write_text('\n'.join(logs))
 report={'project':'nesynth','product':product,'package':output.name,'sha256':sha(raw),
  'bytes':len(raw),'app_sha256':sha(app),'loader_sha256':sha(loader),'sdk_hashes':SDK_HASHES,
  'restore_package':str(restore),'restore_sha256':sha(restore.read_bytes()),
  'dual_core':True,'cartridge':a.label,'private_cartridge':a.label not in ['diagnostic','diagnostic-sram','template','library'],'hardware_validated':False,'installed':False,
  'validation':['SDK hashes','loader RAM placement','LZ4 round trip','encrypted app round trip','OTA simulated flash tests','loader simulated flash tests'],
  'library_capacity':manifest.get('library_capacity',0),'note':'Private NESynth trial. Package writes app sectors; game save RAM is volatile. No public distribution.'}
 (out/('trial-'+a.label+'.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
