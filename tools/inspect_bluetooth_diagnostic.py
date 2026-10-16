"""Read fixed Bluetooth diagnostic metadata over USB; never starts radio or writes flash."""
import argparse,json,re,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire
FIELDS=['state','error','rf_result','rf_bank','rf_record','rf_records','read_result','read_count','read_offset','read_size','sfc_con','bank0_header','bank1_header','private_factory_match']
PREFIX=bytes([0xf0,0x7d,ord('N'),ord('B'),2])

def decode(packet):
    if len(packet)!=6+5*len(FIELDS) or packet[:5]!=PREFIX or packet[-1]!=0xf7 or any(b>127 for b in packet[5:-1]):
        raise ValueError('Invalid Bluetooth diagnostic packet')
    values={name:sum(packet[5+i*5+j]<<(j*7) for j in range(5)) for i,name in enumerate(FIELDS)}
    if any(v>0xffffffff for v in values.values()):raise ValueError('Value exceeds uint32')
    for name in ['rf_result','read_result']:
        if values[name]&0x80000000:values[name]-=0x100000000
    return values

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path)
    args=p.parse_args()
    wire.PORT_RE=re.compile(r'smk|nesynth|ota|composite|sinco|usb-midi',re.I)
    up=wire.Updater(wire.MidoBackend());dev=up.find(lambda i:i.text=='SMK-37_964')
    if not dev:raise RuntimeError('No SMK-37_964 diagnostic replied')
    try:
        dev.link.drain()
        if not dev.link.send(PREFIX+b'\xf7'):raise RuntimeError('USB send failed')
        deadline=time.monotonic()+3
        while time.monotonic()<deadline:
            packet=dev.link.read(.2)
            if packet and packet[:5]==PREFIX:
                result={'identity':dev.id.text,**decode(packet)}
                if args.output:args.output.write_text(json.dumps(result,indent=2)+'\n')
                print(json.dumps(result,indent=2));return
        raise RuntimeError('No Bluetooth status reply; the initial diagnostic build lacks this query')
    finally:dev.link.close()
if __name__=='__main__':main()
