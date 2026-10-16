"""Build a bounded RAM-only SMK updater. No device access."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PRODUCT='SMK-37_961'
def build(tc,out):
    gen=out/'update';gen.mkdir(exist_ok=True)
    rel=lambda p:p.relative_to(ROOT).as_posix()
    src='vendor/smk-updater'
    (gen/'nes_update_config.h').write_text('#define NP_UPDATE_PRODUCT "'+PRODUCT+'"\n')
    (gen/'smk37_unit.h').write_text('#define SMK37_PUBLIC_BUILD 1\n#define SMK37_UNIT_HEAD_CRC 0x3a04ad23u\n#define SMK37_UNIT_TRIM_CRC 0u\n')
    def run(tool,*args):return subprocess.check_output(['wsl','-e',tc+'/'+tool,*map(str,args)],cwd=ROOT,text=True)
    flags=['-target','pi32v2','-Os','-ffunction-sections','-fdata-sections','-fno-builtin','-I'+src,'-Ivendor/felucca/firmware/hal','-I'+rel(gen)]
    for source,name in [('crt0_ldr.S','start'),('smk37_loader.c','loader')]:
        run('pi32v2/bin/clang',*flags,'-c',src+'/'+source,'-o',rel(gen/(name+'.o')))
    elf=rel(gen/'loader.elf')
    run('pi32v2/bin/ld','--gc-sections','-e','_start','-T',src+'/loader.ld',rel(gen/'start.o'),rel(gen/'loader.o'),'-o',elf)
    syms=run('common/bin/objdump','-t',elf);sections=run('common/bin/objdump','-h',elf)
    (gen/'symbols.txt').write_text(syms);(gen/'sections.txt').write_text(sections)
    for name in ['_start','_bss_end','fl_read_ram','fl_prog_ram','fl_erase4k_ram','smku_session']:
        match=re.search(r'^([0-9a-f]+) .*\s'+name+'$',syms,re.M)
        if not match or not 0x1c0a800<=int(match[1],16)<0x1c1e800:raise ValueError('Loader RAM symbol invalid: '+name)
    for line in sections.splitlines():
        p=line.split()
        if len(p)>5 and p[0].isdigit() and int(p[2],16) and p[1] not in ('.symtab','.strtab','.shstrtab'):
            a,n=int(p[3],16),int(p[2],16)
            if not 0x1c0a800<=a<=a+n<=0x1c1e800:raise ValueError('Loader section outside RAM: '+line)
    if re.search(r'\b(?:ota_session|ota_commit|ota_stage)\b',syms):raise ValueError('Flash staging must not link')
    run('common/bin/objcopy','-O','binary','-j','.text',elf,rel(gen/'loader.bin'))
    data=(gen/'loader.bin').read_bytes()
    assert len(data)<=0x14000
    body=PRODUCT.encode().ljust(27,b'\0');msg=bytes([0,0x59,0x11,27,0,0])+body+bytes([~sum(body)&255])
    packed=[];acc=bits=0
    for b in msg:
        acc|=b<<bits;bits+=8
        while bits>=7:packed.append(acc&127);acc>>=7;bits-=7
    if bits:packed.append(acc&127)
    def array(name,raw):return 'static const uint8_t '+name+'[]={'+','.join(map(str,raw))+'};\n'
    (gen/'nes_loader_image.h').write_text(array('np_loader_image',data)+array('np_identity_wire',[0xf0,*packed,0xf7]))
    report={'product':PRODUCT,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'entry':'0x01c0a800','write_lo':'0x4000','write_hi':'0x9d000','flash_staging':False}
    (gen/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return gen,report
