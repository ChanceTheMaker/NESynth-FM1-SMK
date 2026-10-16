"""Make reviewed freestanding patches to pristine pinned InfoNES sources."""
from pathlib import Path
from patch_audio import patch_audio
import shutil
ROOT = Path(__file__).resolve().parents[1]

def prepare():
    dest = ROOT/'build/infones'
    shutil.copytree(ROOT/'vendor/infones',dest,dirs_exist_ok=True)
    def edit(name, before, after):
        path=dest/name
        text=path.read_text(encoding='utf-8-sig')
        if before not in text:
            raise RuntimeError(f'Upstream patch no longer applies: {name}: {before[:65]}')
        path.write_text(text.replace(before,after),encoding='utf-8')
    edit('InfoNES_Types.h','typedef unsigned long  DWORD;','#include <stdint.h>\ntypedef uint32_t DWORD;')
    edit('K6502.h','typedef unsigned long  DWORD;','#include <stdint.h>\ntypedef uint32_t DWORD;')
    edit('InfoNES_pAPU.h','  long time;','  int32_t time;')
    edit('K6502.cpp','( K6502_Read( PC++ ) | (WORD)K6502_Read( PC++ ) << 8 )',
         '([](){ BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC++); return (WORD)(lo | ((WORD)hi<<8)); }())')
    edit('K6502.cpp','( K6502_Read( PC++ ) | (WORD)K6502_Read( PC ) << 8 )',
         '([](){ BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC); return (WORD)(lo | ((WORD)hi<<8)); }())')
    edit('InfoNES.h','#define RAM_SIZE     0x2000','#define RAM_SIZE     0x800')
    # Keep 16-bit palette/transparency arithmetic on one scanline, not a full frame.
    for f in ['InfoNES.h','InfoNES.cpp']:
        edit(f,'WORD WorkFrame[ NES_DISP_WIDTH * NES_DISP_HEIGHT ];','WORD WorkFrame[ NES_DISP_WIDTH ];')
    edit('InfoNES.cpp','&WorkFrame[ PPU_Scanline * NES_DISP_WIDTH ]','WorkFrame')
    edit('InfoNES.cpp','    InfoNES_DrawLine();','    InfoNES_DrawLine();\n    np_capture_line(PPU_Scanline, WorkFrame);')
    edit('InfoNES.cpp','InfoNES_Mirroring( ROM_Mirroring );','InfoNES_Mirroring( ROM_FourScr ? 4 : ROM_Mirroring );')
    edit('InfoNES_System.h','#include "InfoNES_Types.h"','#include "InfoNES_Types.h"\nvoid np_capture_line(unsigned line, const WORD *pixels);\nvoid np_core_error(unsigned code);')
    # CPU state + RAM fit in small RAM; decoded tiles and event storage use POOL.
    edit('InfoNES_Types.h','#endif /* !InfoNES_TYPES_H_INCLUDED */',
         '#if defined(GB_HARDWARE)\n#define NES_POOL __attribute__((section(".pool"), aligned(4)))\n#else\n#define NES_POOL\n#endif\n#endif /* !InfoNES_TYPES_H_INCLUDED */')
    edit('InfoNES.cpp','BYTE ChrBuf[ 256 * 2 * 8 * 8 ];','BYTE ChrBuf[ 256 * 2 * 8 * 8 ] NES_POOL;')
    edit('InfoNES_pAPU.h','#define APU_EVENT_MAX   15000','#define APU_EVENT_MAX   8192')
    edit('InfoNES_pAPU.cpp','ApuEventQueue[ APU_EVENT_MAX ];','ApuEventQueue[ APU_EVENT_MAX ] NES_POOL;')
    edit('InfoNES_pAPU.cpp','  ApuEventQueue[cur_event].time =',
         '  if (cur_event >= APU_EVENT_MAX) { np_core_error(8); return; } \\\n  ApuEventQueue[cur_event].time =')
    # Fix upstream array indexing and prevent writes through ROM-backed PPU banks.
    for f in ['InfoNES.cpp','K6502_rw.h']:
        edit(f,'APU_Reg[ 0x4015 ]','APU_Reg[ 0x15 ]')
    edit('K6502_rw.h','if ( addr < 0x3f00 )','if ( addr >= 0x2000 && addr < 0x3f00 )')
    edit('K6502_rw.h','NesPalette[ byData ]','NesPalette[ byData & 0x3f ]')
    # Sprite path must always start at the current line, including BG-disabled cases.
    edit('InfoNES.cpp','pPoint -= ( NES_DISP_WIDTH - PPU_Scr_H_Bit );','pPoint = WorkFrame;')
    edit('InfoNES.cpp','for ( pSPRRAM = SPRRAM + ( 63 << 2 ); pSPRRAM >= SPRRAM; pSPRRAM -= 4 )',
         'for ( int sprIndex = 63; sprIndex >= 0; --sprIndex )')
    edit('InfoNES.cpp','      nY = pSPRRAM[ SPR_Y ] + 1;','      pSPRRAM = SPRRAM + sprIndex * 4;\n      nY = pSPRRAM[ SPR_Y ] + 1;')
    # MMC2 watches actual pattern high-byte reads, after the triggering tile.
    # Decoded rows are 8 pixels; a raw tile has 16 bytes (two 8-byte planes).
    edit('InfoNES.cpp','MapperPPU( PATTBL( pbyChrData ) );',
         'MapperPPU( ((pbyChrData-ChrBuf)/64)*16 + (((pbyChrData-ChrBuf)/8)&7) + 8 );')
    edit('K6502_rw.h','PPU_R7 = PPUBANK[ addr >> 10 ][ addr & 0x3ff ];',
         'PPU_R7 = PPUBANK[ addr >> 10 ][ addr & 0x3ff ];\n        MapperPPU(addr);')
    # MMC2 sprite fetches need OAM order; preserve the first sprite at each pixel.
    edit('InfoNES.cpp','pSPRRAM = SPRRAM + sprIndex * 4;',
         'pSPRRAM = SPRRAM + (MapperNo == 9 ? 63-sprIndex : sprIndex) * 4;')
    edit('InfoNES.cpp','      ++nSprCnt;',
         '      ++nSprCnt;\n      if (MapperNo == 9 && nSprCnt > 8) continue;')
    for i in range(8):
        dest_pixel='nX' if i==0 else f'nX + {i}'
        # Both flipped and unflipped sprite paths.
        for source_pixel in [i,7-i]:
            before=f'if ( pbyChrData[ {source_pixel} ] )\n          pSprBuf[ {dest_pixel} ]'
            after=f'if ( pbyChrData[ {source_pixel} ] && (MapperNo != 9 || !pSprBuf[ {dest_pixel} ]) )\n          pSprBuf[ {dest_pixel} ]'
            edit('InfoNES.cpp',before,after)
    edit('InfoNES.cpp','      }\n    }\n\n    // Rendering sprite',
         '      }\n      if (MapperNo == 9) MapperPPU( ((pbyChrData-ChrBuf)/64)*16 + (((pbyChrData-ChrBuf)/8)&7) + 8 );\n    }\n\n    // Rendering sprite')
    # Correct gross NTSC scanline pacing; the scanline renderer remains approximate.
    edit('InfoNES.h','#define SCAN_VBLANK_START             243','#define SCAN_VBLANK_START             241')
    edit('InfoNES.h','#define SCAN_VBLANK_END               262','#define SCAN_VBLANK_END               261')
    edit('InfoNES.cpp','  for (;;)\n  {    \n    int nStep;',
         '  static unsigned cycleFraction;\n  for (unsigned lines = 0; lines < 262; ++lines)\n  {\n    cycleFraction += 341;\n    unsigned lineSteps = cycleFraction / 3;\n    cycleFraction %= 3;\n    int nStep;')
    # Only cycle-loop occurrences, leaving the other core timing constants intact.
    p=dest/'InfoNES.cpp'; t=p.read_text(); a=t.index('void InfoNES_Cycle()'); b=t.index('int InfoNES_HSync()',a)
    t=t[:a]+t[a:b].replace('STEP_PER_SCANLINE','lineSteps')+t[b:]; p.write_text(t)
    # Let the current CPU instruction observe vblank before accepting its NMI.
    # Immediate scanline-boundary dispatch starves programs polling $2002 while
    # their NMI handler also reads it (e.g. Ms. Pac-Man's startup).
    # This is a bounded instruction-level approximation, not cycle-exact PPU timing.
    # https://www.nesdev.org/wiki/Consistent_frame_synchronization
    edit('InfoNES.cpp','BYTE PPU_R2;','BYTE PPU_R2;\nstatic BYTE np_vblank_nmi_pending;')
    edit('InfoNES.cpp','  PPU_R0 = PPU_R1 = PPU_R2 = PPU_R3 = PPU_R7 = 0;',
         '  PPU_R0 = PPU_R1 = PPU_R2 = PPU_R3 = PPU_R7 = 0;\n  np_vblank_nmi_pending = 0;')
    edit('InfoNES.cpp','      if ( PPU_R0 & R0_NMI_VB )\n        NMI_REQ;',
         '      if ( PPU_R0 & R0_NMI_VB )\n        np_vblank_nmi_pending = 1;')
    edit('InfoNES.cpp','    int nStep;\n\n    // Set a flag',
         '    int nStep;\n    const unsigned fullLineSteps = lineSteps;\n'
         '    if (np_vblank_nmi_pending) {\n'
         '      np_vblank_nmi_pending = 0;\n'
         '      { unsigned started=np_profile_now(); K6502_Step(2); np_profile_end(0,started); }\n'
         '      lineSteps -= 2;\n'
         '      if ((PPU_R0 & R0_NMI_VB) && (PPU_R2 & R2_IN_VBLANK)) { NMI_REQ; }\n'
         '    }\n\n    // Set a flag')
    edit('InfoNES.cpp','    FrameStep += lineSteps;','    FrameStep += fullLineSteps;')
    edit('K6502_rw.h','#if 0\n        // Reset a V-Blank flag','#if 1\n        // Reset a V-Blank flag')
    edit('InfoNES_pAPU.cpp','ApuC5Freq = ApuC5Phaseacc;','ApuC5Freq = ApuC5Phaseacc = 0;\n  ApuC3Index = ApuC3Skip = 0;')
    for name in ['ApuC1EnvPhase','ApuC2EnvPhase','ApuC4EnvPhase','ApuC1SweepPhase','ApuC2SweepPhase']:
        edit('InfoNES_pAPU.cpp','DWORD '+name+';','int32_t '+name+';')
    # Keep the hot interpreter in existing executable SRAM on the instrument.
    patch_audio(edit,dest)
    # Reset the CPU clock before establishing the APU event epoch.
    edit('InfoNES.cpp','  InfoNES_pAPUInit();','  // APU initialization follows the CPU clock reset below.')
    edit('InfoNES.cpp','  K6502_Reset();','  K6502_Reset();\n  InfoNES_pAPUInit();')
    edit('InfoNES_System.h','void np_core_error(unsigned code);',
         'void np_core_error(unsigned code);\nextern unsigned np_profile_totals[3];\n#if defined(GB_HARDWARE)\nstatic inline unsigned np_profile_now(){return *(volatile unsigned*)0x10804u;}\nstatic inline void np_profile_end(unsigned slot,unsigned start){np_profile_totals[slot]+=np_profile_now()-start;}\n#else\nstatic inline unsigned np_profile_now(){return 0;}\nstatic inline void np_profile_end(unsigned,unsigned){}\n#endif')
    for call in ['K6502_Step( nStep );','K6502_Step( lineSteps - nStep );','K6502_Step( lineSteps );']:
        edit('InfoNES.cpp',call,'{ unsigned started=np_profile_now(); '+call+' np_profile_end(0,started); }')
    edit('InfoNES.cpp','    InfoNES_DrawLine();\n    np_capture_line(PPU_Scanline, WorkFrame);',
         '    unsigned started=np_profile_now();\n    InfoNES_DrawLine();\n    np_capture_line(PPU_Scanline, WorkFrame);\n    np_profile_end(1,started);')
    edit('InfoNES.cpp','        InfoNES_pAPUVsync();','        { unsigned started=np_profile_now(); InfoNES_pAPUVsync(); np_profile_end(2,started); }')
    # XIP/SRAM exceed pi32v2's short-call range: public entry points use explicit
    # indirect calls; static memory helpers remain beside the interpreter.
    edit('K6502.h','#define K6502_H_INCLUDED',
         '#define K6502_H_INCLUDED\n#if defined(GB_HARDWARE) && defined(NES_CPU_TRANSLATION)\n#define NES_CPU_RAM __attribute__((section(".dsp_text")))\n#else\n#define NES_CPU_RAM\n#endif')
    edit('K6502.cpp','#include "K6502.h"','#define NES_CPU_TRANSLATION 1\n#include "K6502.h"')
    for f in ['K6502.h','K6502.cpp','K6502_rw.h']:
        edit(f,'static inline','static inline NES_CPU_RAM')
    edit('K6502.cpp','([](){ BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC++); return (WORD)(lo | ((WORD)hi<<8)); }())','np_cpu_read_operand()')
    edit('K6502.cpp','([](){ BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC); return (WORD)(lo | ((WORD)hi<<8)); }())','np_cpu_peek_operand()')
    edit('K6502.cpp','#include "InfoNES_System.h"',
         '#include "InfoNES_System.h"\nstatic inline NES_CPU_RAM WORD np_cpu_read_operand();\nstatic inline NES_CPU_RAM WORD np_cpu_peek_operand();')
    edit('K6502.cpp','#include "K6502_rw.h"',
         '#include "K6502_rw.h"\nstatic inline NES_CPU_RAM WORD np_cpu_read_operand() { BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC++); return (WORD)(lo|((WORD)hi<<8)); }\nstatic inline NES_CPU_RAM WORD np_cpu_peek_operand() { BYTE lo=K6502_Read(PC++); BYTE hi=K6502_Read(PC); return (WORD)(lo|((WORD)hi<<8)); }')
    signatures=[('Init','', ''),('Reset','', ''),('Set_Int_Wiring','BYTE byNMI_Wiring, BYTE byIRQ_Wiring','byNMI_Wiring, byIRQ_Wiring'),('Step','WORD wClocks','wClocks')]
    wrappers=[]
    for name,args,values in signatures:
        signature=f'void K6502_{name}( {args} )' if args else f'void K6502_{name}()'
        edit('K6502.cpp',signature+'\n{',
             (f'static void NES_CPU_RAM np_cpu_{name}( {args} )' if args else f'static void NES_CPU_RAM np_cpu_{name}()')+'\n{')
        types=', '.join(part.split()[0] for part in args.split(', ')) if args else ''
        wrappers.append(signature+' { void (*volatile fn)('+types+') = np_cpu_'+name+'; fn('+values+'); }')
    p=dest/'K6502.cpp';p.write_text(p.read_text()+'\n'+'\n'.join(wrappers)+'\n')
    edit('K6502_rw.h','#include "InfoNES_pAPU.h"',
         '#include "InfoNES_pAPU.h"\nstatic inline NES_CPU_RAM void np_cpu_copy(void *dst,const void *src,int count) { BYTE *d=(BYTE*)dst; const BYTE *s=(const BYTE*)src; while(count--) *d++=*s++; }\nstatic inline NES_CPU_RAM void np_cpu_report(unsigned code) { void (*volatile fn)(unsigned)=np_core_error; fn(code); }\n#define np_core_error np_cpu_report')
    edit('K6502_rw.h','InfoNES_MemoryCopy( SPRRAM,','np_cpu_copy( SPRRAM,')
    edit('K6502_rw.h','InfoNES_pAPUWriteControl( wAddr, byData );',
         '{ void (*volatile fn)(WORD,BYTE)=ApuWriteControl; fn(wAddr,byData); }')
    # The renderer and CHR decoder are also hot on the instrument. Keep their
    # loops in executable SRAM and use indirect wrappers across the XIP boundary.
    edit('InfoNES.cpp','void InfoNES_DrawLine()\n{',
         '#if defined(GB_HARDWARE)\n#define NES_PPU_RAM __attribute__((section(".dsp_text")))\n#else\n#define NES_PPU_RAM\n#endif\nstatic inline NES_PPU_RAM void np_ppu_clear(void *p,int v,int n){ BYTE *d=(BYTE*)p; while(n--) *d++=(BYTE)v; }\nstatic void NES_PPU_RAM np_ppu_draw()\n{')
    p=dest/'InfoNES.cpp';t=p.read_text();a=t.index('static void NES_PPU_RAM np_ppu_draw()');b=t.index('void InfoNES_GetSprHitY()',a)
    t=t[:a]+t[a:b].replace('InfoNES_MemorySet(', 'np_ppu_clear(')+t[b:]
    t=t.replace('void InfoNES_SetupChr()\n{','static void NES_PPU_RAM np_ppu_decode()\n{')
    t+='\nvoid InfoNES_DrawLine(){ void (*volatile fn)()=np_ppu_draw; fn(); }\nvoid InfoNES_SetupChr(){ void (*volatile fn)()=np_ppu_decode; fn(); }\n'
    p.write_text(t)
    # MMC2 can switch banks several times per scanline. Decoding all 4 KiB on
    # each latch is wasteful; fetch just the two pattern bytes for a used row.
    edit('InfoNES.cpp','static void NES_PPU_RAM np_ppu_draw()', '''static inline NES_PPU_RAM void np_ppu_row(BYTE *row){
  if (MapperNo != 9) return;
  unsigned offset=(unsigned)(row-ChrBuf);
  unsigned address=(offset/64)*16+(offset/8)%8;
  const BYTE *source=PPUBANK[address>>10]+(address&1023);
  BYTE lo=source[0],hi=source[8];
  for(unsigned x=0;x<8;x++)row[x]=((lo>>(7-x))&1)|(((hi>>(7-x))&1)<<1);
}
static void NES_PPU_RAM np_ppu_draw()''')
    p=dest/'InfoNES.cpp';t=p.read_text();a=t.index('static void NES_PPU_RAM np_ppu_draw()');b=t.index('void InfoNES_GetSprHitY()',a)
    part=t[a:b]
    # Every background row is fetched before reading its colors; sprites share
    # one fetch after selecting their 8x8/8x16 address and before flipping pixels.
    import re
    part,count=re.subn(r'(pbyChrData = PPU_BG_Base[^;]+;)',r'\1\n    np_ppu_row(pbyChrData);',part)
    assert count==4
    part=part.replace('      nAttr ^= SPR_ATTR_PRI;','      np_ppu_row(pbyChrData);\n      nAttr ^= SPR_ATTR_PRI;')
    t=t[:a]+part+t[b:];p.write_text(t)
    edit('InfoNES.cpp','static void NES_PPU_RAM np_ppu_decode()\n{','static void NES_PPU_RAM np_ppu_decode()\n{\n  if (MapperNo == 9) return;')
    # Sprite-zero hit estimation reads decoded rows outside the draw path too.
    edit('InfoNES.cpp','if ( pdwChrData[ 0 ] | pdwChrData[ 1 ] )',
         '{ void (*volatile fetch)(BYTE*)=np_ppu_row; fetch((BYTE*)pdwChrData); }\n\t\t\tif ( pdwChrData[ 0 ] | pdwChrData[ 1 ] )')
    # Skipped raster frames still execute PPU mapper fetches and sprite counts.
    # CPU, vblank, sprite-zero estimation and APU run on every emulated frame.
    edit('InfoNES.cpp','void InfoNES_DrawLine(){ void (*volatile fn)()=np_ppu_draw; fn(); }', '''extern unsigned np_skip_pixels;
static void NES_PPU_RAM np_ppu_fetch_only(){
  if (MapperNo==9 && (PPU_R1&R1_SHOW_SCR)) {
    unsigned table=PPU_NameTableBank,y=PPU_Scr_V_Byte+(PPU_Scanline>>3);
    unsigned fine=PPU_Scr_V_Bit+(PPU_Scanline&7);
    if(fine>7){++y;fine&=7;}
    if(y>29){table^=NAME_TABLE_V_MASK;y-=30;}
    unsigned x=PPU_Scr_H_Byte,base=(unsigned)(PPU_BG_Base-ChrBuf)/4;
    for(unsigned i=0;i<33;i++){
      MapperPPU(base+PPUBANK[table][y*32+x]*16+fine+8);
      if(++x==32){x=0;table^=NAME_TABLE_H_MASK;}
    }
  }
  if(PPU_R1&R1_SHOW_SP){
    PPU_R2&=~R2_MAX_SP;unsigned count=0;
    for(unsigned i=0;i<64;i++){
      const BYTE *sp=SPRRAM+(MapperNo==9?i:63-i)*4;
      unsigned y=sp[SPR_Y]+1;
      if(y>PPU_Scanline || y+PPU_SP_Height<=PPU_Scanline)continue;
      ++count;if(MapperNo!=9 || count>8)continue;
      unsigned row=PPU_Scanline-y;
      if(sp[SPR_ATTR]&SPR_ATTR_V_FLIP)row=PPU_SP_Height-1-row;
      unsigned address;
      if(PPU_R0&R0_SP_SIZE)address=(sp[SPR_CHR]&1)*4096+(sp[SPR_CHR]&254)*16;
      else address=(unsigned)(PPU_SP_Base-ChrBuf)/4+sp[SPR_CHR]*16;
      MapperPPU(address+(row/8)*16+(row&7)+8);
    }
    if(count>=8)PPU_R2|=R2_MAX_SP;
  }
}
void InfoNES_DrawLine(){ void (*volatile fn)()=np_skip_pixels?np_ppu_fetch_only:np_ppu_draw; fn(); }''')
    # Immutable MMC2 CHR ROM allows a row cache keyed by its source address.
    # 16 KiB on pi32v2, bounded by the existing shared-pool linker assertion.
    edit('InfoNES.cpp','static inline NES_PPU_RAM void np_ppu_row(BYTE *row){',
         'static const BYTE *np_row_source[4096] NES_POOL;\nstatic void np_ppu_reset_rows(){ InfoNES_MemorySet(np_row_source,0,sizeof np_row_source); }\nstatic inline NES_PPU_RAM void np_ppu_row(BYTE *row){')
    edit('InfoNES.cpp','  BYTE lo=source[0],hi=source[8];',
         '  if(np_row_source[offset/8]==source)return;\n  np_row_source[offset/8]=source;\n  BYTE lo=source[0],hi=source[8];')
    edit('InfoNES.cpp','  K6502_Reset();','  np_ppu_reset_rows();\n  K6502_Reset();')
    edit('InfoNES.cpp','int InfoNES_Reset()', 'static void np_ppu_reset_rows();\nint InfoNES_Reset()')
    # Mapper 0 has no PPU callbacks; MMC2 only cares about FD/FE high-byte reads.
    p=dest/'InfoNES.cpp';t=p.read_text().replace('MapperPPU(', 'np_ppu_notify(')
    t=t.replace('static const BYTE *np_row_source[4096] NES_POOL;', '''static inline NES_PPU_RAM void np_ppu_notify(unsigned address){
  if(MapperNo==9 && (address==0xfd8 || address==0xfe8 ||
      (address&0x1ff8)==0x1fd8 || (address&0x1ff8)==0x1fe8)) MapperPPU((WORD)address);
}
static const BYTE *np_row_source[4096] NES_POOL;''')
    # Neither supported mapper uses MMC5's rendering-mode notification.
    t=t.replace('MapperRenderScreen( 1 );','/* Mappers 0/9: no rendering-mode callback. */').replace('MapperRenderScreen( 0 );','/* Mappers 0/9: no rendering-mode callback. */')
    p.write_text(t)
    # MMC1 shares the fetched-row path, including banked CHR RAM (uncached).
    edit('InfoNES.cpp','if (MapperNo != 9) return;','if (MapperNo != 9 && MapperNo != 1 && MapperNo != 3 && MapperNo != 4) return;')
    edit('InfoNES.cpp','if (MapperNo == 9) return;','if (MapperNo == 9 || MapperNo == 1 || MapperNo == 3 || MapperNo == 4) return;')
    edit('InfoNES.cpp','  if(np_row_source[offset/8]==source)return;\n  np_row_source[offset/8]=source;',
         '  if(NesHeader.byVRomSize){\n    if(np_row_source[offset/8]==source)return;\n    np_row_source[offset/8]=source;\n  }')
    edit('K6502_rw.h','#include "InfoNES_pAPU.h"','#include "InfoNES_pAPU.h"\nextern BYTE np_prg_ram_enabled,np_prg_ram_writable,np_mapper_irq_pending;')
    edit('K6502_rw.h','return SRAMBANK[ wAddr & 0x1fff ];','return np_prg_ram_enabled ? SRAMBANK[ wAddr & 0x1fff ] : 0xff;')
    edit('K6502_rw.h','return SRAM[ wAddr & 0x1fff ];','return np_prg_ram_enabled ? SRAM[ wAddr & 0x1fff ] : 0xff;')
    edit('K6502_rw.h','SRAM[ wAddr & 0x1fff ] = byData;','if(np_prg_ram_enabled && np_prg_ram_writable) SRAM[ wAddr & 0x1fff ] = byData;')
    # A mapper IRQ is a level: accepting it does not acknowledge the MMC3.
    edit('K6502.cpp','#include "K6502.h"','#include "K6502.h"\nextern BYTE np_mapper_irq_pending;')
    edit('K6502.cpp','IRQ_State = IRQ_Wiring;', 'IRQ_State = np_mapper_irq_pending ? 0 : IRQ_Wiring;')
    # This CPU core omits RMW dummy writes. MMC1 needs the original write and
    # ignores the next data bit, but a reset bit on the second write still acts.
    edit('K6502.cpp','#include "K6502_rw.h"', '''#include "K6502_rw.h"
static inline NES_CPU_RAM void np_cpu_rmw(WORD address,BYTE oldValue,BYTE newValue){
  if(MapperNo==1 && address>=0x8000){
    K6502_Write(address,oldValue);
    if(newValue&128)K6502_Write(address,newValue);
  }else K6502_Write(address,newValue);
}''')
    edit('K6502.cpp','#include "InfoNES_System.h"','#include "InfoNES_System.h"\nstatic inline NES_CPU_RAM void np_cpu_rmw(WORD,BYTE,BYTE);')
    p=dest/'K6502.cpp';t=p.read_text();lines=t.splitlines()
    for i,line in enumerate(lines):
        if any(line.startswith('#define '+name+'(a)') for name in ['DEC','INC','ASL','LSR','ROL','ROR']):
            line=line.replace('byD0 = K6502_Read( wA0 );','byD0 = K6502_Read( wA0 ); BYTE oldValue=byD0;')
            line=line.replace('K6502_Write( wA0,','np_cpu_rmw( wA0, oldValue,')
            # Keep the temporary scoped inside its macro expansion.
            pos=line.index(')')+1;line=line[:pos]+' { '+line[pos:]+'; }'
            lines[i]=line
    p.write_text('\n'.join(lines)+'\n')
    return dest

if __name__=='__main__': prepare()

