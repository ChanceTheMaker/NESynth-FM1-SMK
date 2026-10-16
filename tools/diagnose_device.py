"""Read-only NESynth FM-1 and SMK diagnostics, including CPU1 and frame timing."""
import argparse,json,re,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire
FIELDS=['uptime_ms','frames','queued_audio','audio_irqs','scan_irqs','master_raw','master_gain','keys','core_error','alnk_con0','alnk_con2','display_frames','audio_underruns','panel_buttons','game_buttons','game_buttons_seen','cpu1_online','cpu1_jobs','cpu1_fallbacks','cpu1_faults','cpu1_barred','cpu1_stage','cpu1_core_id','cpu1_launch_con','cpu1_launch_pc','cpu1_launch_emu','cpu1_launch_dbg','cpu1_fault_pc','cpu1_fault_emu','cpu1_fault_dbg','cpu1_launch_ready','cpu1_retire_reason','cpu1_retire_ticks','cpu1_worker_jobs','cpu1_work_max_ticks','emu_last_ticks','emu_max_ticks','emu_total_ticks','emu_measured_frames','lcd_max_ticks','cpu_ticks','ppu_ticks','apu_ticks','scan_irq_ticks']
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--board',choices=['fm1','smk37'],default='fm1')
    p.add_argument('--port',help='Restrict discovery to a MIDI port name')
    p.add_argument('--output',type=Path);p.add_argument('--samples',type=int,default=3)
    p.add_argument('--interval',type=float,default=1);p.add_argument('--version',type=int,choices=[3,4,5])
    a=p.parse_args()
    if a.samples<1 or a.interval<0:p.error('Samples must be positive and interval nonnegative')
    a.version=a.version or (5 if a.board=='smk37' else 4)
    if a.board=='fm1' and a.version==5:p.error('Version 5 requires SMK-37_962 or later')
    a.output=a.output or ROOT/'build'/a.board/'diagnostics.json'
    fields=FIELDS[:40] if a.version==3 else FIELDS[:]
    if a.version==5:fields+=['lcd_busy','lcd_timeouts','lcd_failed','usb_irq_ticks','clock_div','clock_sys']
    if a.board=='smk37':wire.PORT_RE=re.compile(r'smk|sinco|usb-midi|composite|ota',re.I)
    model='SMK-37' if a.board=='smk37' else 'FM-1'
    up=wire.Updater(wire.MidoBackend(),port=a.port);dev=up.find(lambda i:i.model==model)
    if not dev:raise RuntimeError('No '+model+' NESynth identity response')
    rows=[];prefix=bytes([0xf0,0x7d,ord('N'),ord('S'),a.version])
    try:
        print('Identity:',dev.id.text,flush=True)
        for i in range(a.samples):
            dev.link.drain();dev.link.send(prefix+b'\xf7');deadline=time.monotonic()+3;reply=None
            while time.monotonic()<deadline:
                candidate=dev.link.read(.2)
                if candidate and candidate[:5]==prefix:reply=candidate;break
            if not reply or len(reply)!=6+len(fields)*5 or reply[-1]!=0xf7:raise RuntimeError('No complete NESynth diagnostics response'+('; install SMK-37_962 or later first' if a.board=='smk37' else ''))
            if any(b>127 for b in reply[5:-1]):raise RuntimeError('Invalid diagnostics encoding')
            row={name:sum(reply[5+k*5+j]<<(7*j) for j in range(5)) for k,name in enumerate(fields)}
            rows.append(row);print(json.dumps(row),flush=True)
            if i+1<a.samples:time.sleep(a.interval)
        result={'identity':dev.id.text,'samples':rows}
        if len(rows)>1:
            first,last=rows[0],rows[-1];seconds=((last['uptime_ms']-first['uptime_ms'])&0xffffffff)/1000
            if not seconds:raise RuntimeError('Device uptime did not advance; increase --interval')
            result['rates']={name:((last[field]-first[field])&0xffffffff)/seconds for name,field in [('emulation_fps','frames'),('display_fps','display_frames'),('cpu1_jobs_per_second','cpu1_jobs'),('audio_underruns_per_second','audio_underruns')]}
            n=(last['emu_measured_frames']-first['emu_measured_frames'])&0xffffffff
            result['mean_frame_ms']=((last['emu_total_ticks']-first['emu_total_ticks'])&0xffffffff)/24000/max(1,n)
            print(json.dumps({'rates':result['rates'],'mean_frame_ms':result['mean_frame_ms']}),flush=True)
        if a.version>=4 and len(rows)>1:
            profile=['cpu_ticks','ppu_ticks','apu_ticks','scan_irq_ticks']+(['usb_irq_ticks'] if a.version==5 else [])
            result['profile_ms_per_frame']={field:((last[field]-first[field])&0xffffffff)/24000/max(1,n) for field in profile}
            print(json.dumps(result['profile_ms_per_frame']),flush=True)
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps(result,indent=2)+'\n')
    finally:dev.link.close()
if __name__=='__main__':main()
