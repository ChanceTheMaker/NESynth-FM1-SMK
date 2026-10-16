"""Build the integrated SMK Bluetooth player, diagnostic, or BLE size probes.

Requires the unchanged Hortator checkout and its hash-matched AC79 SDK.
Default profiles are measurement-only. --diagnostic builds a separate SMK964
raw application. --loader builds SMK967 game upload and playback.
Neither mode flashes, packages, or publishes automatically.
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
    p.add_argument('--peripheral',action='store_true',help='Link the SMK diagnostic GATT profile instead of the reference central')
    p.add_argument('--loader',action='store_true',help='Build integrated SMK967 Bluetooth cartridge loader and NES player')
    p.add_argument('--diagnostic',action='store_true',help='Build local SMK964 radio diagnostic; never flashes or publishes')
    p.add_argument('--private-calibration-backups',nargs=2,type=Path,help='PRIVATE device build only: paired stock backups; exact validated usrtrim match required')
    p.add_argument('--private-unit-backups',nargs=2,type=Path,help='Optional paired current 8192-byte factory reads; only after confirming the RF backup belongs to this same unit')
    a=p.parse_args()
    if a.loader and a.diagnostic:p.error('Choose loader or diagnostic')
    installable=a.diagnostic or a.loader
    if a.private_calibration_backups and not installable:p.error('Private calibration requires --diagnostic or --loader')
    if a.private_unit_backups and not a.private_calibration_backups:p.error('Current unit reference requires private RF backups')
    a.peripheral=a.peripheral or installable
    ref=a.reference.resolve();out=ROOT/('build/bluetooth-loader' if a.loader else 'build/bluetooth-diagnostic' if a.diagnostic else 'build/bluetooth-peripheral-probe' if a.peripheral else 'build/bluetooth-probe');out.mkdir(parents=True,exist_ok=True)
    (out/'report.json').unlink(missing_ok=True) # A failed run must not leave a success report.
    def run(args,**kw):return subprocess.run(list(map(str,args)),cwd=ROOT,check=True,**kw)
    if a.private_calibration_backups:
        extractor=out/'extract-private-rf'
        run(['wsl','-e','cc','-O1','-Wall','-Wextra','-Werror','tools/extract_private_rf.c','experiments/bluetooth/rf_calibration.c','-o',wsl(extractor)])
        run(['wsl','-e',wsl(extractor),*[wsl(x) for x in a.private_calibration_backups],wsl(out/'private_calibration.h'),*[wsl(x) for x in (a.private_unit_backups or [])]])
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
    if installable:
        import build_smk_loader
        build_smk_loader.PRODUCT='SMK-37_967' if a.loader else 'SMK-37_964'
        build_smk_loader.build(a.toolchain,player)
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
    if a.peripheral:
        edit('ble/sdkcfg/app_config.h','#define BT_NET_CENTRAL_EN               1','#define BT_NET_CENTRAL_EN               0')
        edit('ble/sdkcfg/app_config.h','#define TRANS_DATA_EN                   0','#define TRANS_DATA_EN                   1')
        q=out/'ble/ble.c';q.write_text(q.read_text().split('#include "ble/ble_central.c"')[0]+'#include "diagnostic_peripheral.c"\n')
        q=out/'ble/ble_port.c';t=q.read_text();start=t.index('#define BLE_VM_FROM');end=t.index('uint8_t RTDebugLevel',start)
        t=t[:start]+'''#include "rf_calibration.h"
static np_rf_calibration np_rf;
static int np_rf_result;
uint32_t np_ble_rf_status(unsigned i){return i==0?(uint32_t)np_rf_result:i==1?np_rf.bank:i==2?np_rf.record:np_rf.records;}
static int np_rf_read_adapter(void *ctx,uint32_t off,uint8_t *p,uint32_t n){
    (void)ctx;core_wdt_feed();return core_flash_read(off,p,n);
}
static int ble_rf_trim_load(void){
    ble_rf_trim_at=0;
    np_rf_result=np_rf_load_smk(np_rf_read_adapter,0,&np_rf);
    if(np_rf_result)return 0;
    memcpy(ble_rf_trim,np_rf.trim,sizeof ble_rf_trim);ble_rf_trim_at=np_rf.record;return 1;
}
'''+t[end:]
        if a.private_calibration_backups:
            t=t.replace('static int np_rf_result;', 'static int np_rf_result;\n#include "private_calibration.h"\nextern int core_private_calibration_matches(const unsigned char *,unsigned);')
            t=t.replace('if(np_rf_result)return 0;', 'if(np_rf_result){\n        if(!core_private_calibration_matches(np_private_factory,sizeof np_private_factory)){np_rf_result=-5;return 0;}\n        np_rf=np_private_rf;np_rf_result=0;\n    }')
        t=t.replace('xosc[0] = 0x0B; xosc[1] = 0x0B;','xosc[0] = np_rf.xosc[0]; xosc[1] = np_rf.xosc[1];')
        t=t.replace('memcpy(pa, PA, 7);','memcpy(pa, np_rf.pa, 7);')
        t=t.replace('int cpu_in_irq(void) { return 0; }','int cpu_in_irq(void) { unsigned v; __asm__ volatile("%0 = icfg" : "=r"(v)); return v & 255u; }')
        t=t.replace('int cpu_irq_disabled(void) { return fm1_ble_irq_depth != 0; }','int cpu_irq_disabled(void) { unsigned a,b; __asm__ volatile("%0 = icfg" : "=r"(a)); __asm__ volatile("%0 = icfg" : "=r"(b)); return fm1_ble_irq_depth || ((a & 0x300u) != 0x300u && (b & 0x300u) != 0x300u); }')
        t+='\nuint32_t np_ble_runtime_status(unsigned i){if(ble_diag.magic!=BLE_DIAG_MAGIC)return 0;return i==0?ble_diag.stage:i==1?ble_diag.trim:i==2?ble_diag.fatal:i==3?ble_diag.heap_high:ble_diag.alloc_fails;}\n'
        t+='void np_ble_private_probe(void){'+('core_private_calibration_matches(np_private_factory,sizeof np_private_factory);' if a.private_calibration_backups else '')+'}\n'
        q.write_text(t)
    if a.loader:
        edit('ble/ble.c','diagnostic_peripheral.c','loader_peripheral.c')
    # Only the generated board copy exports the buffer; production source is unchanged.
    board=(ROOT/'firmware/board.c').read_text()+'\nunsigned char *np_ble_probe_workspace(void){return video_source;}\n'
    if installable:
        board='extern unsigned np_ble_diag_state(void);\nstatic int np_ble_usb(const unsigned char *,unsigned);\n'+board
        board=board.replace('void gb_cstart(const uint32_t *parameters){','#include "diagnostic_board.h"\nvoid gb_cstart(const uint32_t *parameters){')
        if not a.loader:board=board.replace('int display_ok=display_init();','display_init();input_init();recovery_init();fm1_irq_enable_all();np_diag_run();\nint display_ok=0;')
        recovery=(ROOT/'firmware/smk37_recovery.h').read_text().replace('static void recovery_enter(void){','static void recovery_enter(void){\n    if(np_ble_diag_state()){fm1_reboot();for(;;);}')
        recovery=recovery.replace('int identity=n==sizeof query', 'if(np_ble_usb(p,n)){ota_frame_done();return;}\n        int identity=n==sizeof query')
        if a.loader:recovery=recovery.replace('if(np_ble_diag_state()){fm1_reboot();for(;;);}', 'if(np_ble_diag_state()){np_loader_recover();for(;;);}').replace('video_retire();','video_stop();')
        (out/'smk37_recovery.h').write_text(recovery)
    if a.loader:
        # Version the noinit guard once when moving from the diagnostic/player.
        # Intentional mode changes stop CPU1 without latching its fault guard.
        (out/'multicore_video.h').write_text((ROOT/'firmware/multicore_video.h').read_text(encoding='utf-8').replace('0x47445634u','0x47445635u'),encoding='utf-8')
        # Stage, LCD strip and radio workspace alias the inactive gameplay pool.
        # Every transition to gameplay/flash installation passes through chip reset.
        board=board.replace('return video_source;', 'return (unsigned char *)0x01c65000u;')
        board=board.replace('static int np_ble_usb', 'static void np_loader_recover(void);\nstatic int np_loader_usb(const unsigned char *,unsigned);\nstatic int np_ble_usb',1)
        board=board.replace('#include "diagnostic_board.h"', '#define NP_BLUETOOTH_LOADER 1\n#define FL_RANGE_OK(off,n) ((uint32_t)(off)>=0x39000u && (uint32_t)(off)<=0x9d000u && (uint32_t)(n)<=0x9d000u-(uint32_t)(off))\n#include "diagnostic_board.h"\n#include "loader_board.h"')
        board=board.replace('zero(_bss_start,_bss_end);zero(_pool_start,_pool_end);','zero(_bss_start,_bss_end);')
        board=board.replace('fm1_irq_init();fm1_mailbox_clear();','fm1_irq_init();fm1_mailbox_clear();\n    np_loader_boot(cold);zero(_pool_start,_pool_end);')
        board=board.replace('int display_ok=display_init();','int display_ok=display_init();\n    if(np_loader_mode==2){recovery_init();recovery_enter();}\n    if(np_loader_mode){input_init();recovery_init();fm1_irq_enable_all();np_loader_run();}')
        board=board.replace('const volatile uint8_t *cart_header=cartridge;', 'const uint8_t *active_cart=np_installed?np_installed:cartridge;\n    uint32_t active_size=np_installed?np_installed_length:sizeof cartridge;\n    const volatile uint8_t *cart_header=active_cart;')
        board=board.replace('np_rom_size(cart_header,sizeof cartridge)','np_rom_size(cart_header,active_size)').replace('cart_size>sizeof cartridge||gp_load(cartridge,cart_size)','cart_size>active_size||gp_load(active_cart,cart_size)')
        board=board.replace('        recovery_service();','        recovery_service();np_loader_controls();')
        rom=(ROOT/'build/diagnostic.nes').read_bytes()
        (out/'cartridge.h').write_text('static const unsigned char cartridge[]={'+','.join(map(str,rom))+'};\n')
    (out/'board.c').write_text(board)
    cc=['wsl','-e',a.toolchain+'/pi32v2/bin/clang']
    flags=['-target','pi32v2','-Os','-fno-builtin','-ffunction-sections','-fdata-sections']
    def compile(src,dest,extra=()):
        run([*cc,*flags,*extra,'-c',wsl(src),'-o',wsl(dest)])
    compile(out/'board.c',out/'board.o',['-mllvm','-pi32v2-large-program=true','-Dmemcpy=np_core_memcpy','-DGB_HARDWARE=1','-DMUSICCOACH_DUAL_CORE=1','-DGB_SMK37=1',
        '-Isrc','-Isrc/freestanding','-Ifirmware','-Ivendor/felucca/firmware/hal','-Ibuild/smk37','-Ibuild/smk37/update',
        '-I'+wsl(ROOT/'experiments/bluetooth'),'-I'+wsl(ref/'firmware/hal')])
    compile(out/'ble/ble.c',out/'ble.o',['-mllvm','-pi32v2-large-program=true','-I'+wsl(out),'-I'+wsl(ref/'firmware/hal'),'-I'+wsl(ROOT/'experiments/bluetooth')])
    compile(ref/'firmware/hal/fm1_ctx.S',out/'ctx.o')
    if not installable:compile(ROOT/'experiments/bluetooth/probe_hooks.c',out/'hooks.o')
    compile(ROOT/'experiments/bluetooth/rom_transfer.c',out/'receiver.o',['-mllvm','-pi32v2-large-program=true'])
    peripheral=[]
    if a.peripheral:
        for name in ['rf_calibration','diagnostic_gatt']:
            obj=out/(name+'.o');source=ROOT/('experiments/bluetooth/'+name+'.c')
            if a.loader and name=='diagnostic_gatt':
                source=out/'loader_gatt.c'
                source.write_text((ROOT/'experiments/bluetooth/diagnostic_gatt.c').read_text().replace('0x0a,6,0','0x0e,6,0').replace('0x0a,3,6,0','0x0e,3,6,0').replace('0xe0','0xf0').replace('0xe1','0xf1').replace('NP_BLE_NAME','\"NESynth SMK\"').replace("17,9,'N','E','S','y','n','t','h',' ','S','M','K',' ','T','e','s','t'","12,9,'N','E','S','y','n','t','h',' ','S','M','K'"))
            compile(source,obj,['-mllvm','-pi32v2-large-program=true','-Iexperiments/bluetooth']);peripheral.append(obj)
    cfg=[]
    for source in deps.SDK_CONFIG_SOURCES:
        dest=out/(Path(source).stem+'.o')
        options=[('-I'+wsl(deps.SDK)+s[len('-I/sdk'):] if s.startswith('-I/sdk/') else s.replace('firmware/src/ble/sdkcfg',wsl(out/'ble/sdkcfg'))) for s in deps.SDK_CFLAGS]
        config_source=deps.SDK/source
        if a.loader and Path(source).name=='lib_btctrler_config.c':
            # Adapt a generated copy only, after verifying the pinned SDK source.
            config_source=out/'loader_btctrler_config.c'
            config_text=(deps.SDK/source).read_text(encoding='utf-8')
            old_features='config_btctler_le_features = (LE_ENCRYPTION);'
            old_length='config_btctler_le_acl_packet_length = 27;'
            assert config_text.count(old_features)==config_text.count(old_length)==1
            config_text=config_text.replace(old_features,'config_btctler_le_features = (LE_ENCRYPTION | LE_DATA_PACKET_LENGTH_EXTENSION);').replace(old_length,'config_btctler_le_acl_packet_length = 251;')
            config_source.write_text(config_text,encoding='utf-8')
        compile(config_source,dest,options);cfg.append(dest)
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
    if a.loader:script+='\nASSERT(_data_load + SIZEOF(.data) <= 0x02057000, \"Application overlaps cartridge metadata\")\nASSERT(0x01c1a000+262672 <= 0x01c5b000, \"ROM staging overlaps LCD strip\")\nASSERT(0x01c5b000+240*64*2 <= 0x01c65000, \"LCD strip overlaps BLE\")\nASSERT(0x01c65000+23744 <= 0x01c74000, \"BLE overlaps stack\")\n'
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
    objects=[*core,out/'board.o',out/'ble.o',out/'ctx.o',*([out/'hooks.o'] if not installable else []),out/'receiver.o',out/'lto_stubs.o',*cfg,*members,*peripheral]
    profile_roots=['np_ble_diag_start','np_ble_diag_service','np_ble_diag_state','np_ble_diag_error','np_ble_diag_writes'] if a.peripheral else ['ble_user_start','ble_service','ble_console','ble_midi_send']
    elf=out/('nesynth-SMK967-BLUETOOTH.elf' if a.loader else 'nesynth-SMK964-BLE-DIAGNOSTIC.elf' if a.diagnostic else 'nesynth-BLE-PROBE-DO-NOT-FLASH.elf')
    run(['wsl','-e',a.toolchain+'/pi32v2/bin/ld','--gc-sections','--plugin',a.toolchain+'/pi32v2/bin/LLVMgold.so',
         '--plugin-opt=mcpu=r3','--plugin-opt=-pi32v2-large-program=true','--plugin-opt=-pi32v2-always-use-itblock=false',
         '--plugin-opt=-dont-used-symbol-list='+','.join(deps.LOG_FUNCS),'--orphan-handling=error','--wrap=memcpy',
         *['--undefined='+x for x in profile_roots+['np_ble_workspace_used','np_rx_init','np_rx_begin','np_rx_data','np_rx_commit','np_rx_abort']],
         '-T',wsl(out/'probe.ld'),'-Map',wsl(out/'probe.map'),*[wsl(x) for x in objects],
         '--start-group',*[wsl(x) for x in libs],'--end-group','-o',wsl(elf)])
    def dump(*args):return subprocess.check_output(['wsl','-e',a.toolchain+'/common/bin/objdump',*args,wsl(elf)],text=True)
    syms=dump('-t');sections=dump('-h');(out/'symbols.txt').write_text(syms);(out/'sections.txt').write_text(sections)
    # Same RAM reach check as Hortator tools/build.py rt_reach (GPL-3.0-only).
    # Flash reads and radio trim must not call XIP or use XIP constants.
    ram_dis=dump('-d','-j','.ram_text');(out/'ram-disassembly.txt').write_text(ram_dis)
    for line in ram_dis.splitlines():
        if not re.match(r'^\s*[0-9a-f]+:',line):continue
        if re.search(r'\b(call|goto)\s+r\d+\b',line):raise ValueError('Indirect control transfer in RAM: '+line)
        for target in re.findall(r'<[^>]*:\s*([0-9a-f]+)\s*>',line):
            addr=int(target,16)
            if 0x02000000<=addr<0x02100000 or (re.search(r'\b(call|goto)\b',line) and not 0x1c00000<=addr<0x1c06000):
                raise ValueError('RAM code reaches outside RAM: '+line)
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
    assert sym('_start')==0x2000120,'Unexpected application entry address'
    result={'kind':'SMK964 diagnostic candidate' if a.diagnostic else 'non-installable link probe','reference':PIN,'rom_capacity':0 if a.diagnostic else a.rom_capacity,
        'bluetooth_role':'diagnostic peripheral, RAM echo only' if a.peripheral else 'upstream central, phone peripheral still required','flash_writes':False,'radio_started':False,
        'app_bytes':size,'app_limit':581564,'app_free':581564-size,'baseline_app_bytes':baseline['app_bytes'],
        'ble_added_bytes':size-baseline['app_bytes'],'ram_bytes':sym('_bss_end')-0x1c08000,'ram_limit':73728,
        'pool_and_cpu1_stacks_bytes':sym('_cpu1_stacks_hi')-0x1c1a000,'pool_limit':368640,
        'ram_code_bytes':sym('_dsp_end')-0x1c00000,'ram_code_limit':24576,
        'ble_workspace_used_bytes':constant('np_ble_workspace_used'),
        'shared_workspace_bytes':61440,'shared_workspace':'inactive video_source; simultaneous gameplay/BLE is forbidden',
        'sdk_hashes':deps.PINS,'sdk_config_lf_hashes':deps.SOURCE_PINS,
        'limitations':['Probe hooks only; no device transport or real radio startup','No cartridge file-transfer service','No Bluetooth package or binary image is emitted']}
    assert size<=581564 and result['ram_bytes']<=73728 and result['pool_and_cpu1_stacks_bytes']<=368640
    assert result['ble_workspace_used_bytes']<=61440 and result['ram_code_bytes']<=24576
    if installable:
        for name in ['np_loader_image','usb_start','gb_isr_usb','fl_read_ram']:
            assert re.search(r'\s'+name+'$',syms,re.M),'Missing recovery/read symbol: '+name
        for name in ([] if a.loader else ['fl_erase4k_ram','fl_prog_ram']):
            body=ram_dis.split(name+':\n')[1].split('\n\n')[0]
            assert re.search(r'r0 = -1\b',body) and not re.search(r'\b(call|goto)\b|\]\s*=',body),'Flash writer was not disabled'
        binary=out/('SMK967-BLUETOOTH-RAW.bin' if a.loader else 'SMK964-BLE-DIAGNOSTIC-RAW.bin')
        data=bytearray()
        for section,address in [('.text',0x2000120),('.ram_text',sym('_rt_load')),('.dsp_text',sym('_dsp_load')),('.data',sym('_data_load'))]:
            part=out/(section[1:]+'.bin');part.unlink(missing_ok=True)
            run(['wsl','-e',a.toolchain+'/common/bin/objcopy','-O','binary','-j',section,wsl(elf),wsl(part)])
            blob=part.read_bytes() if part.exists() else b''
            if blob:
                offset=address-0x2000120;assert offset>=len(data)
                data.extend(b'\xff'*(offset-len(data)));data.extend(blob)
        assert len(data)==size and len(data)%4==0;binary.write_bytes(data)
        result.update(separate_current_unit_reference=bool(a.private_unit_backups),private_device_calibration=bool(a.private_calibration_backups),product='SMK-37_964',sha256=hashlib.sha256(data).hexdigest(),hardware_validated=False,
            baseline_app_bytes=None,ble_added_bytes=None,ram_reach_check=True,application_flash_writers_disabled=True,
            limitations=['Radio starts only after a fresh physical PLAY press','No game playback or cartridge upload in this diagnostic','No hardware test performed; power-cycle before USB restore'])
    if a.loader:
        assert size<=0x39000-0x4120, 'Application overlaps game library'
        result.update(library_capacity=0x9d000-0x3b000,library_slots=32,controller_packet_bytes=251,controller_data_length_extension=True,adapted_controller_config_sha256=hashlib.sha256((out/'loader_btctrler_config.c').read_bytes()).hexdigest(),app_limit=0x39000-0x4120,app_free=0x39000-0x4120-size,kind='SMK967 Bluetooth loader and player',product='SMK-37_967',rom_capacity=262672,bluetooth_role='cartridge loader',flash_writes=True,application_flash_writers_disabled=False,cartridge_flash_lo=0x39000,cartridge_flash_hi=0x9d000,shared_workspace='exclusive loader mode: ROM 0x1c1a000, LCD 0x1c5b000, BLE 0x1c65000',limitations=['Maximum cartridge size 262672 bytes','Radio active only in loader mode','Hardware validation pending'])
    (out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
