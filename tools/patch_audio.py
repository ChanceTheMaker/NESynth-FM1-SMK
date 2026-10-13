"""Fix retained InfoNES APU buffer, envelope and note-release defects.

Reference: https://www.nesdev.org/wiki/APU_Envelope and APU_registers.
The renderer remains sample-based, with approximate frame-length timing.
"""
def patch_audio(edit,dest):
    # The CPU's remainder is not elapsed time. Preserve a monotonic scanline base.
    edit('K6502.h','extern WORD g_wPassedClocks;','extern WORD g_wPassedClocks;\nextern DWORD np_cpu_clock_base;')
    edit('K6502.cpp','WORD g_wPassedClocks;','WORD g_wPassedClocks;\nDWORD np_cpu_clock_base;')
    edit('K6502.cpp','g_wPassedClocks = 0;','g_wPassedClocks = 0;\n  np_cpu_clock_base = 0;')
    edit('K6502.cpp','g_wPassedClocks -= wClocks;','g_wPassedClocks -= wClocks;\n  np_cpu_clock_base += wClocks;')
    edit('InfoNES_pAPU.cpp','WORD entertime;','DWORD entertime;')
    edit('InfoNES_pAPU.cpp','entertime - g_wPassedClocks','(DWORD)(np_cpu_clock_base + g_wPassedClocks - entertime)')
    edit('InfoNES_pAPU.cpp','entertime = g_wPassedClocks;','entertime = np_cpu_clock_base + g_wPassedClocks;')
    # Breaking out replays last frame's remaining samples and loses later writes.
    for channel in range(3):
        edit('InfoNES_pAPU.cpp',f'wave_buffers[{channel}][i] = 0;\n      break;',f'wave_buffers[{channel}][i] = 0;\n      continue;')
    edit('InfoNES_pAPU.cpp','wave_buffers[4][i] = ( ApuC5Reg[1]&0x01 ) + ( ApuC5DpcmValue << 1 );\n    }',
         'wave_buffers[4][i] = ( ApuC5Reg[1]&0x01 ) + ( ApuC5DpcmValue << 1 );\n    } else { wave_buffers[4][i] = 0; }')
    edit('InfoNES_pAPU.cpp','ApuC5Freq = ApuC5Phaseacc = 0;','ApuC5Freq = 428 << 16; ApuC5Phaseacc = 0;')
    # Bit 4 selects constant volume. Envelope decay must reach silence, not add gain.
    for c in [1,2,4]:
        edit('InfoNES_pAPU.cpp',f'ApuC{c}EnvPhase -= 4;',f'ApuC{c}EnvPhase -= 240;')
        edit('InfoNES_pAPU.cpp',f'ApuC{c}EnvPhase += ApuC{c}EnvDelay;',f'ApuC{c}EnvPhase += ApuC{c}EnvDelay * ApuSampleRate;')
    for c in [1,2]:
        edit('InfoNES_pAPU.cpp',f'if ( ApuC{c}Env )',f'if ( !ApuC{c}Env )')
        edit('InfoNES_pAPU.cpp',f'( ApuC{c}Vol + ApuC{c}EnvVol )',f'( 15 - ApuC{c}EnvVol )')
        edit('InfoNES_pAPU.cpp',f'( ApuC{c}Atl || ApuC{c}Hold )',f'ApuC{c}Atl')
        edit('InfoNES_pAPU.cpp',f'if ( ApuC{c}Atl ) {{ ApuC{c}Atl--;  }}',f'if ( ApuC{c}Atl && !ApuC{c}Hold ) {{ ApuC{c}Atl--; }}')
        edit('InfoNES_pAPU.cpp',f'ApuC{c}SweepPhase -= 2;',f'ApuC{c}SweepPhase -= 120;')
        edit('InfoNES_pAPU.cpp',f'ApuC{c}SweepPhase += ApuC{c}SweepDelay;',f'ApuC{c}SweepPhase += ApuC{c}SweepDelay * ApuSampleRate;')
        # Only timer-high writes reload length/start envelopes; timer-low is pitch.
        p=dest/'InfoNES_pAPU.cpp';s=p.read_text();start=s.index(f'int ApuWriteWave{c}');end=s.index(f'void ApuRenderingWave{c}',start)
        part=s[start:end];reload=f'ApuC{c}Atl = ApuAtl[ ( ApuC{c}d & 0xf8 ) >> 3 ];'
        assert part.count(reload)==2;part=part.replace(reload,'',1)
        part=part.replace(reload,f'ApuC{c}Atl = (ApuCtrlNew & {1<<(c-1)}) ? ApuAtl[(ApuC{c}d >> 3)] : 0;\n          ApuC{c}EnvVol = 0; ApuC{c}EnvPhase = ApuC{c}EnvDelay * ApuSampleRate;')
        s=s[:start]+part+s[end:];p.write_text(s)
    edit('InfoNES_pAPU.cpp','ApuC2Freq -= ~( ApuC2Freq >> ApuC2SweepShifts );','ApuC2Freq -= ( ApuC2Freq >> ApuC2SweepShifts );')
    edit('InfoNES_pAPU.cpp','if ( !ApuC4Env )','if ( ApuC4Env )')
    edit('InfoNES_pAPU.cpp','ApuC4d = ApuEventQueue[event].data;','ApuC4d = ApuEventQueue[event].data;\n        ApuC4EnvVol = 0; ApuC4EnvPhase = ApuC4EnvDelay * ApuSampleRate;')
    p=dest/'InfoNES_pAPU.cpp';s=p.read_text();a=s.index('int ApuWriteWave4');b=s.index('void ApuRenderingWave4',a);part=s[a:b]
    assert part.count('ApuC4Atl = ApuC4LengthCounter;')==2
    part=part.replace('ApuC4Atl = ApuC4LengthCounter;','',1).replace('ApuC4Atl = ApuC4LengthCounter;','ApuC4Atl = (ApuCtrlNew & 8) ? ApuC4LengthCounter : 0;')
    s=s[:a]+part+s[b:];p.write_text(s)
    edit('InfoNES_pAPU.h','#define ApuC4LengthCounter  ( ApuAtl[ ( ApuC4d >> 3 ) ] << 1 )','#define ApuC4LengthCounter  ( ApuAtl[ ApuC4d >> 3 ] )')
    # Triangle linear counter clocks at quarter-frame rate and reloads on $400B.
    edit('InfoNES_pAPU.cpp','BYTE  ApuC3CounterStarted;','BYTE  ApuC3CounterStarted;\nstatic unsigned np_triangle_phase;\nstatic BYTE np_triangle_reload;')
    edit('InfoNES_pAPU.cpp','ApuC3Llc = ApuC3LinearLength;','/* $4008 changes the reload value, not the current counter. */')
    edit('InfoNES_pAPU.cpp','ApuC3Atl = ApuC3LengthCounter;','ApuC3Atl = (ApuCtrlNew & 4) ? ApuC3LengthCounter : 0;\n        np_triangle_reload = 1;')
    p=dest/'InfoNES_pAPU.cpp';s=p.read_text();a=s.index('    /* Counter Control */');b=s.index('    /* Wave Rendering */',a)
    s=s[:a]+'''    np_triangle_phase += 240;
    if (np_triangle_phase >= ApuSampleRate) {
      np_triangle_phase -= ApuSampleRate;
      if (np_triangle_reload) ApuC3Llc = ApuC3a & 127;
      else if (ApuC3Llc) --ApuC3Llc;
      if (!ApuC3Holdnote) np_triangle_reload = 0;
    }

'''+s[b:];a=s.index('void ApuRenderingWave3');b=s.index('int ApuWriteWave4',a);part=s[a:b]
    part=part.replace('( ApuC3Atl > 0 || ApuC3Holdnote )','ApuC3Atl > 0')
    at=part.rfind('\n}');part=part[:at]+'\n  if (ApuC3Atl && !ApuC3Holdnote) --ApuC3Atl;'+part[at:]
    s=s[:a]+part+s[b:];p.write_text(s)
    edit('InfoNES_pAPU.cpp','ApuC3CounterStarted = 0x00;','ApuC3CounterStarted = 0x00;\n  np_triangle_phase = np_triangle_reload = 0;')
    edit('InfoNES_pAPU.cpp','5, 127, 10, 1, 19,','5, 127, 10, 1, 20,')
