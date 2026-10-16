"""Build the NES feasibility prototype; hardware outputs are never install packages."""
import argparse,hashlib,json,os,re,subprocess,sys
from pathlib import Path
from diagnostic import make_rom
from cartridge import read_cartridge
from prepare_core import prepare
ROOT=Path(__file__).resolve().parents[1]
ZIG=str(ROOT.parent/'Felucca/Felucca-website/build/wasm-tools/ziglang/zig.exe')
_tc=ROOT.parent/'Felucca/Felucca-review/.build-deps/jieli/jieli-linux-toolchains-20250324.1'
TC='/mnt/'+_tc.drive[0].lower()+_tc.as_posix()[2:] if _tc.drive else str(_tc)
CPP=['build/infones/InfoNES.cpp','build/infones/K6502.cpp','build/infones/InfoNES_pAPU.cpp','src/mapper.cpp','src/nes_port.cpp']
EXPORTS=['gp_upload','gp_load_upload','gp_frame','gp_buttons','gp_pause','gp_reset','gp_volume','gp_pixels','gp_save_data','gp_save_size','gp_save_dirty','gp_save_clean','gp_audio_queued','gp_render_audio','gp_error','gp_frame_count','np_palette','np_audio_samples','np_render_stride','np_video_frame']
def run(cmd):subprocess.run(list(map(str,cmd)),cwd=ROOT,check=True)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('target',choices=['test','sanitize','web','fm1','smk37']);p.add_argument('--zig',default=os.environ.get('ZIG',ZIG));p.add_argument('--toolchain',default=os.environ.get('JIELI_TOOLCHAIN',TC));p.add_argument('--rom',type=Path,help='Raw .nes or .zip cartridge');p.add_argument('--rom-entry',help='Exact .nes entry name when a ZIP contains several cartridges');p.add_argument('--rom-capacity',type=int,default=65536);a=p.parse_args()
    prepare();(ROOT/'web').mkdir(exist_ok=True);(ROOT/'build/diagnostic.nes').write_bytes(make_rom())
    inc=['-Isrc','-Ibuild/infones'];common=['-O2','-fno-builtin','-fno-strict-aliasing',*inc]
    cppflags=['-std=c++11','-fno-exceptions','-fno-rtti','-Wno-write-strings','-Wno-deprecated-register']
    if a.target=='sanitize':
        run(['wsl','-e','g++','-std=c++11','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-strict-aliasing','-Wno-write-strings',*inc,*CPP,'tests/runtime.cpp','-o','build/runtime-sanitized'])
        run(['wsl','-e','./build/runtime-sanitized','build/diagnostic.nes']);return
    if a.target=='test':
        run([a.zig,'c++',*common,*cppflags,'-UNDEBUG',*CPP,'tests/runtime.cpp','-o','build/runtime-test.exe']);run([ROOT/'build/runtime-test.exe',ROOT/'build/diagnostic.nes'])
        run([a.zig,'cc','-O1','-UNDEBUG','-Isrc','tests/smk37_game_controls.c','-o','build/smk37-controls.exe']);run([ROOT/'build/smk37-controls.exe'])
        run([a.zig,'cc','-O1','-UNDEBUG','tests/smk37_display.c','-o','build/smk37-display.exe']);run([ROOT/'build/smk37-display.exe'])
        run([sys.executable,'tests/diagnostics.py'])
        run([a.zig,'cc','-O1','-UNDEBUG','tests/smk37_scan.c','-o','build/smk37-scan.exe']);run([ROOT/'build/smk37-scan.exe'])
        return
    native=a.target in ['fm1','smk37'];out=ROOT/'build'/a.target;out.mkdir(exist_ok=True)
    sources=CPP+['src/libc.c']
    if native:
        if a.target=='smk37':
            from build_smk_loader import build as build_loader
            gen,loader=build_loader(a.toolchain,out)
            common+=['-I'+gen.relative_to(ROOT).as_posix()]
        rom=read_cartridge(a.rom,a.rom_entry) if a.rom else make_rom()
        if a.rom_capacity<len(rom) or a.rom_capacity>524816:raise ValueError('Invalid ROM capacity')
        storage=rom+b'\xff'*(a.rom_capacity-len(rom))
        (out/'cartridge.h').write_text('static const unsigned char cartridge[]={\n'+',\n'.join(','.join(str(v) for v in storage[i:i+32]) for i in range(0,len(storage),32))+'\n};\n')
        sources+=['firmware/board.c','firmware/start.S','vendor/felucca/firmware/hal/fm1_vec.S']
        common+=['-target','pi32v2','-DGB_HARDWARE=1','-DMUSICCOACH_DUAL_CORE=1','-DGB_SMK37='+str(int(a.target=='smk37')),'-ffunction-sections','-fdata-sections','-Isrc/freestanding','-Ivendor/felucca/firmware/hal','-I'+out.relative_to(ROOT).as_posix()]
    else:
        sources+=['src/wasm.c'];common+=['-target','wasm32-freestanding','-Isrc/freestanding']
    objs=[]
    for i,src in enumerate(sources):
        obj=(out/f'{i}.o').relative_to(ROOT).as_posix();objs.append(obj)
        compiler=['wsl','-e',a.toolchain+'/pi32v2/bin/clang'] if native else [a.zig,'cc']
        opts=cppflags if src.endswith('.cpp') else ['-std=c99'] if src.endswith('.c') else []
        flags=[('-Os' if f=='-O2' and src=='firmware/board.c' else f) for f in common]
        run([*compiler,*flags,*opts,'-c',src,'-o',obj])
    if not native:
        run([a.zig,'cc','-target','wasm32-freestanding','-nostdlib','-Wl,--no-entry','-Wl,-z,stack-size=0x10000','-Wl,--initial-memory=0x400000','-Wl,--max-memory=0x400000',*['-Wl,--export='+x for x in EXPORTS],*objs,'-o','web/nes.wasm'])
        (ROOT/'web/diagnostic.nes').write_bytes(make_rom());blob=(ROOT/'web/nes.wasm').read_bytes();result={'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest(),'mappers':[0,1,2,3,4,9,11],'region':'NTSC','core':'InfoNES'};(ROOT/'web/build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
    elf=(out/'nes.elf').relative_to(ROOT).as_posix()
    run(['wsl','-e',a.toolchain+'/pi32v2/bin/ld','--gc-sections','-T','vendor/felucca/firmware/app.ld',*objs,'-o',elf])
    def capture(*args):return subprocess.check_output(['wsl','-e',a.toolchain+'/common/bin/objdump',*args],cwd=ROOT,text=True)
    syms=capture('-t',elf);(out/'symbols.txt').write_text(syms);(out/'sections.txt').write_text(capture('-h',elf))
    def sym(name):return int(re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M)[1],16)
    image=bytearray()
    for sec,addr in [('.text',0x2000120),('.ram_text',sym('_rt_load')),('.dsp_text',sym('_dsp_load')),('.data',sym('_data_load'))]:
        f=out/(sec[1:]+'.bin');run(['wsl','-e',a.toolchain+'/common/bin/objcopy','-O','binary','-j',sec,elf,f.relative_to(ROOT).as_posix()]);blob=f.read_bytes() if f.exists() else b''
        if blob:
            offset=addr-0x2000120;assert offset>=len(image);image.extend(b'\xff'*(offset-len(image)));image.extend(blob)
    image.extend(b'\xff'*(-len(image)%4));assert len(image)<=0x8dfbc
    assert sym('_bss_end')<=0x1c1a000 and sym('_cpu1_stacks_hi')<=0x1c74000
    for required in ['gb_core1_start','gb_core1_main']:
        assert re.search(r'\s'+required+r'$',syms,re.M),'Missing CPU1 worker: '+required
    assert sym('_cpu1_stacks_hi')-sym('_cpu1_stacks_lo')>=8192,'Missing CPU1 stack reservation'
    if a.target=='fm1':
        for required in ['usb_start','ota_session.parm']:assert re.search(r'\s'+re.escape(required)+r'$',syms,re.M),'Missing USB updater'
    else:
        for required in ['usb_start','gb_isr_usb','np_loader_image']:
            assert re.search(r'\s'+re.escape(required)+r'$',syms,re.M),'Missing SMK updater: '+required
    (out/'nes-NOT-FOR-INSTALL.bin').write_bytes(image)
    result={'board':a.target,'installable':False,'hardware_validated':False,'app_bytes':len(image),'app_limit':0x8dfbc,'rom_capacity':a.rom_capacity,'rom_bytes':len(rom),'ram_bytes':sym('_bss_end')-0x1c08000,'ram_budget':73728,'pool_bytes':sym('_pool_end')-0x1c1a000,'cpu1_stack_bytes':sym('_cpu1_stacks_hi')-sym('_cpu1_stacks_lo'),'pool_budget':368640,'sha256':hashlib.sha256(image).hexdigest(),'dual_core':True,'cpu0_role':'NES CPU/PPU/APU, audio, input, USB, LCD DMA','cpu1_role':'immutable frame scaling and RGB565 conversion','usb_updater':a.target=='fm1','display_target_fps':20,'mappers':[0,1,2,3,4,9,11],'hardware_fps':None}
    result.update(cartridge_offset=sym('cartridge')-0x2000120,cartridge_capacity=a.rom_capacity)
    if a.target=='smk37':result.update(usb_updater=True,usb_protocol='smk37-ram-v2',product=loader['product'],loader=loader)
    (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
