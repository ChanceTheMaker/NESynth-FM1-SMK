"""Read fixed FM-1 NESynth radio metadata over USB without changing device state."""
import json,re,sys,time
from pathlib import Path
from upload_usb_game import wire
FIELDS="state error rf_result rf_bank rf_record rf_records stage trim fatal heap_high alloc_fails irq_leaks mac_lo mac_hi xosc delays timer_stalls irq_lowered sys_hz hsb_hz lsb_hz irqs_lo irqs_hi irq_depth events last_event run_max_ticks tasks irq29_masked irq45_masked".split()
FIELDS += "clock_con0 clock_con1 clock_con2 pll_con0 bb_con0 bb_con1 radio_irq_calls radio_irq_rets radio_irq_icfg radio_power timer4_con timer4_period".split()
FIELDS += "rf_crc32 adv_crc32 rsp_crc32 tx_i_gain irq29_handler irq45_handler".split()
def main():
    wire.PORT_RE=re.compile(r"fm-?1|felucca|nesynth|ota|composite|sinco|usb-midi",re.I)
    dev=wire.Updater(wire.MidoBackend()).find(lambda i:i.text=="FM-1_969")
    if not dev:raise RuntimeError("FM-1_969 not found")
    values=[]
    try:
        for page in range(8):
            prefix=b"\xf0\x7dNL\x05"+bytes([page]);dev.link.drain()
            if not dev.link.send(prefix+b"\xf7"):raise RuntimeError("USB send failed")
            end=time.monotonic()+3
            while time.monotonic()<end:
                p=dev.link.read(.2)
                if not p or not p.startswith(prefix):continue
                if len(p)!=37 or p[-1]!=247 or any(b>127 for b in p[1:-1]):raise ValueError("Invalid radio status packet")
                words=[sum(p[6+i*5+j]<<(7*j) for j in range(5)) for i in range(6)]
                if any(v>0xffffffff for v in words):raise ValueError("Radio status word overflow")
                values.extend(words);break
            else:raise RuntimeError("No radio status response; this build may lack the query")
    finally:dev.link.close()
    result={"identity":dev.id.text,**dict(zip(FIELDS,values))}
    mac=result["mac_lo"].to_bytes(4,"little")+result["mac_hi"].to_bytes(2,"little")
    result["address"]=":".join(f"{b:02X}" for b in reversed(mac))
    print(json.dumps(result,indent=2),flush=True)
    if len(sys.argv)>1:Path(sys.argv[1]).write_text(json.dumps(result,indent=2)+"\n")
if __name__=="__main__":main()
