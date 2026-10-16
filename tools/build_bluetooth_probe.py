"""Link NESynth + pinned Hortator BLE for size measurement. NEVER makes install firmware.

Requires the unchanged Hortator checkout and its hash-matched AC79 SDK.
The central profile is retained for measurement; it is not a phone/file service.
Generated adaptations and SDK objects stay under ignored build/.
Credit: DEADACTIVE's Hortator BLE port (GPL-3.0-only), based on Felucca;
JieLi SDK library/configuration notices remain in their original sources.
"""
import argparse,hashlib,importlib.util,json,re,shutil,struct,subprocess,sys
from pathlib import Path
from build import ROOT,TC,CPP
PIN='7e3415971c5800b4a3ad0146a7d95615883d7d96'

def wsl(path):
    p=Path(path).resolve()
    return '/mnt/'+p.drive[0].lower()+p.as_posix()[2:]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference',type=Path,default=ROOT/'build/references/hortator')
    p.add_argument('--sdk',type=Path,default=ROOT/'build/references/ac79-ble-sdk')
    p.add_argument('--toolchain',default=TC)
    p.add_argument('--rom-capacity',type=int,choices=[262672,393744,524816],default=262672)
    a=p.parse_args();ref=a.reference.resolve();out=ROOT/'build/bluetooth-probe';out.mkdir(parents=True,exist_ok=True)
    (out/'report.json').unlink(missing_ok=True) # A failed run must not leave a success report.
    def run(args,**kw):return subprocess.run(list(map(str,args)),cwd=ROOT,check=True,**kw)
    head=subprocess.check_output(['git','-C',str(ref),'rev-parse','HEAD'],text=True).strip()
    if head!=PIN:raise ValueError('Hortator revision does not match audited source')
    run(['git','-C',ref,'diff','--exit-code','HEAD','--','firmware','tools/ble_libs.py'])
    spec=importlib.util.spec_from_file_location('hortator_ble_libs',ref/'tools/ble_libs.py')
    deps=importlib.util.module_from_spec(spec);spec.loader.exec_module(deps)
    deps.SDK=a.sdk.resolve()
    def sdk_sha(path):
        data=path.read_bytes()
        # Windows checkouts can have CRLF. Normalize only the two pinned text
        # sources, without changing the reference checkout or any library bytes.
        if path.relative_to(deps.SDK).as_posix() in deps.SOURCE_PINS:
            data=data.replace(b'\r\n',b'\n')
        return hashlib.sha256(data).hexdigest()
    deps.sha=sdk_sha
    libs=deps.libs() # verifies every library and config source
    members=deps.members(out)
    run([sys.executable,'tools/build.py','smk37','--rom-capacity',a.rom_capacity,'--toolchain',a.toolchain],stdout=subprocess.DEVNULL)
    player=ROOT/'build/smk37';baseline=json.loads((player/'manifest.json').read_text())
    shutil.copytree(ref/'firmware/src/ble',out/'ble',dirs_exist_ok=True)
    def edit(file,old,new):
        q=out/file;t=q.read_text();assert t.count(old)==1,(file,old);q.write_text(t.replace(old,new))
    # Real sized task structs share an inactive 60 KiB frame snapshot, no linker overlap.
    edit('ble/ble.c','#include "ble/ble_os.c"',
         'extern unsigned char *np_ble_probe_workspace(void);\n#define NP_BLE_HEAP_BYTES 13824u\n#include "ble/ble_os.c"')
    edit('ble/ble_os.c','static task_t tasks[NTASK] BLE_POOL __attribute__((aligned(8)));',
         '#define tasks ((task_t *)(void *)(np_ble_probe_workspace()+NP_BLE_HEAP_BYTES))\n'
         '_Static_assert(NP_BLE_HEAP_BYTES+NTASK*sizeof(task_t)<=61440u,"loader workspace overflow");\n'
         'const unsigned np_ble_workspace_used=NP_BLE_HEAP_BYTES+NTASK*sizeof(task_t);')
    edit('ble/ble.c','static uint8_t ble_heap[BLE_HEAP_BYTES] __attribute__((section(".pool")));',
         '#define ble_heap (np_ble_probe_workspace())')
    q=out/'ble/ble_central.c';q.write_text(q.read_text().replace('sizeof ble_heap','BLE_HEAP_BYTES'))
    # Only the generated board copy exports the buffer; production source is unchanged.
    board=(ROOT/'firmware/board.c').read_text()+'\nunsigned char *np_ble_probe_workspace(void){return video_source;}\n'
    (out/'board.c').write_text(board)
    cc=['wsl','-e',a.toolchain+'/pi32v2/bin/clang']
    flags=['-target','pi32v2','-Os','-fno-builtin','-ffunction-sections','-fdata-sections']
    def compile(src,dest,extra=()):
        run([*cc,*flags,*extra,'-c',wsl(src),'-o',wsl(dest)])
    compile(out/'board.c',out/'board.o',['-mllvm','-pi32v2-large-program=true','-Dmemcpy=np_core_memcpy','-DGB_HARDWARE=1','-DMUSICCOACH_DUAL_CORE=1','-DGB_SMK37=1',
        '-Isrc','-Isrc/freestanding','-Ifirmware','-Ivendor/felucca/firmware/hal','-Ibuild/smk37','-Ibuild/smk37/update'])
    compile(out/'ble/ble.c',out/'ble.o',['-mllvm','-pi32v2-large-program=true','-I'+wsl(out),'-I'+wsl(ref/'firmware/hal')])
    compile(ref/'firmware/hal/fm1_ctx.S',out/'ctx.o')
    compile(ROOT/'experiments/bluetooth/probe_hooks.c',out/'hooks.o')
    compile(ROOT/'experiments/bluetooth/rom_transfer.c',out/'receiver.o',['-mllvm','-pi32v2-large-program=true'])
    cfg=[]
    for source in deps.SDK_CONFIG_SOURCES:
        dest=out/(Path(source).stem+'.o')
        options=[('-I'+wsl(deps.SDK)+s[len('-I/sdk'):] if s.startswith('-I/sdk/') else s.replace('firmware/src/ble/sdkcfg',wsl(out/'ble/sdkcfg'))) for s in deps.SDK_CFLAGS]
        compile(deps.SDK/source,dest,options);cfg.append(dest)
    compile(out/'ble/ble_lto_stubs.c',out/'lto_stubs.o',['-mcpu=r3','-flto'])
    script=(ref/'firmware/app_ble.ld').read_text().replace('LENGTH = 96K','LENGTH = 72K').replace('ORIGIN = 0x01C20000, LENGTH = 0x54000','ORIGIN = 0x01C1A000, LENGTH = 0x5A000')
    script=script.replace('    _rt_load = LOADADDR(.ram_text);','''    _rt_load = LOADADDR(.ram_text);
    .dsp_text : { _dsp_start = .; *(.dsp_text*) . = ALIGN(4); _dsp_end = .; } > RAMTEXT AT > XIP
    _dsp_load = LOADADDR(.dsp_text);''')
    script=script.replace('    .noinit (NOLOAD)', '''    .cpu1_stacks (NOLOAD) : {
        . = ALIGN(32); _cpu1_stacks_lo = .; KEEP(*(.cpu1_stacks)) _cpu1_stacks_hi = .;
    } > POOL
    _cpu1_ustack_top = _cpu1_stacks_lo + 4096;
    _cpu1_sstack_top = _cpu1_stacks_lo + 8192;
    ASSERT(_cpu1_stacks_hi <= 0x01c74000, "CPU1 stacks exceed SRAM pool")
    .noinit (NOLOAD)''')
    (out/'probe.ld').write_text(script)
    # Root the complete central service and the ROM receiver size, even though no startup calls them.
    # Only BLE calls the RAM memcpy wrapper. Preserve the player's existing
    # short XIP calls by giving its memcpy a private name in probe-only objects.
    core=[]
    for i,source in enumerate(CPP+['src/libc.c','firmware/start.S','vendor/felucca/firmware/hal/fm1_vec.S']):
        dest=out/('core-'+str(i)+'.o')
        extra=['-O2','-fno-strict-aliasing','-Dmemcpy=np_core_memcpy','-DGB_HARDWARE=1','-DMUSICCOACH_DUAL_CORE=1','-DGB_SMK37=1',
               '-Isrc','-Ibuild/infones','-Isrc/freestanding','-Ivendor/felucca/firmware/hal']
        if source.endswith('.cpp'):extra+=['-std=c++11','-fno-exceptions','-fno-rtti','-Wno-write-strings','-Wno-deprecated-register']
        compile(ROOT/source,dest,extra)
        core.append(dest)
    objects=[*core,out/'board.o',out/'ble.o',out/'ctx.o',out/'hooks.o',out/'receiver.o',out/'lto_stubs.o',*cfg,*members]
    elf=out/'nesynth-BLE-PROBE-DO-NOT-FLASH.elf'
    run(['wsl','-e',a.toolchain+'/pi32v2/bin/ld','--gc-sections','--plugin',a.toolchain+'/pi32v2/bin/LLVMgold.so',
         '--plugin-opt=mcpu=r3','--plugin-opt=-pi32v2-large-program=true','--plugin-opt=-pi32v2-always-use-itblock=false',
         '--plugin-opt=-dont-used-symbol-list='+','.join(deps.LOG_FUNCS),'--orphan-handling=error','--wrap=memcpy',
         *['--undefined='+x for x in ['ble_user_start','ble_service','ble_console','ble_midi_send','np_ble_workspace_used','np_rx_init','np_rx_begin','np_rx_data','np_rx_commit','np_rx_abort']],
         '-T',wsl(out/'probe.ld'),'-Map',wsl(out/'probe.map'),*[wsl(x) for x in objects],
         '--start-group',*[wsl(x) for x in libs],'--end-group','-o',wsl(elf)])
    def dump(*args):return subprocess.check_output(['wsl','-e',a.toolchain+'/common/bin/objdump',*args,wsl(elf)],text=True)
    syms=dump('-t');sections=dump('-h');(out/'symbols.txt').write_text(syms);(out/'sections.txt').write_text(sections)
    def sym(name):return int(re.search(r'^([0-9a-f]+) .*\s'+re.escape(name)+r'$',syms,re.M)[1],16)
    def constant(name):
        data=elf.read_bytes();address=sym(name)
        assert data[:6]==b'\x7fELF\x01\x01', 'Expected little-endian ELF32'
        section_offset=struct.unpack_from('<I',data,32)[0]
        section_size,section_count=struct.unpack_from('<HH',data,46)
        for index in range(section_count):
            _,kind,_,base,offset,length=struct.unpack_from('<6I',data,section_offset+index*section_size)
            if kind==1 and base<=address and address+4<=base+length:
                return struct.unpack_from('<I',data,offset+address-base)[0]
        raise ValueError('Constant is not in a file-backed ELF section: '+name)
    size=sym('_data_load')+sym('_data_end')-sym('_data_start')-0x2000120
    result={'kind':'non-installable link probe','reference':PIN,'rom_capacity':a.rom_capacity,
        'bluetooth_role':'upstream central, phone peripheral still required','flash_writes':False,'radio_started':False,
        'app_bytes':size,'app_limit':581564,'app_free':581564-size,'baseline_app_bytes':baseline['app_bytes'],
        'ble_added_bytes':size-baseline['app_bytes'],'ram_bytes':sym('_bss_end')-0x1c08000,'ram_limit':73728,
        'pool_and_cpu1_stacks_bytes':sym('_cpu1_stacks_hi')-0x1c1a000,'pool_limit':368640,
        'ram_code_bytes':sym('_dsp_end')-0x1c00000,'ram_code_limit':24576,
        'ble_workspace_used_bytes':constant('np_ble_workspace_used'),
        'shared_workspace_bytes':61440,'shared_workspace':'inactive video_source; simultaneous gameplay/BLE is forbidden',
        'sdk_hashes':deps.PINS,'sdk_config_lf_hashes':deps.SOURCE_PINS,
        'limitations':['Probe hooks only; no device transport or real radio startup','No peripheral/file-transfer service','No Bluetooth package or binary image is emitted']}
    assert size<=581564 and result['ram_bytes']<=73728 and result['pool_and_cpu1_stacks_bytes']<=368640
    assert result['ble_workspace_used_bytes']<=61440 and result['ram_code_bytes']<=24576
    (out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
