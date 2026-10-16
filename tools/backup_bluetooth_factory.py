"""Private fixed-region backup from SMK964 with radio off; no writes or radio start."""
import argparse,hashlib,json,re,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire
PREFIX=bytes([0xf0,0x7d,78,66,4])

def decode(packet,chunk):
    if not packet or len(packet)<8 or packet[:5]!=PREFIX or packet[5]!=chunk or packet[-1]!=0xf7 or any(v>127 for v in packet[1:-1]):raise ValueError('Invalid factory reply')
    status=packet[6]
    if status:
        if len(packet)!=8 or status not in [1,2]:raise ValueError('Invalid factory error reply')
        return status,None
    if len(packet)!=136 or any(packet[8+i*2]>1 for i in range(64)):raise ValueError('Invalid factory payload')
    return 0,bytes(packet[7+i*2]|packet[8+i*2]<<7 for i in range(64))

def read_factory(link):
    result=bytearray()
    for chunk in range(128):
        until=time.monotonic()+4
        while time.monotonic()<until:
            link.drain()
            if not link.send(PREFIX+bytes([chunk,0xf7])):raise RuntimeError('Disconnected')
            end=time.monotonic()+.5;reply=None
            while time.monotonic()<end:
                p=link.read(.1)
                if p and p[:5]==PREFIX and len(p)>5 and p[5]==chunk:reply=p;break
            if reply:
                status,data=decode(reply,chunk)
                if not status:result.extend(data);break
                if status==2:raise RuntimeError('Factory read failed')
            time.sleep(.02)
        else:raise RuntimeError('Factory reader busy or unavailable; radio must be off')
    return bytes(result)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output-dir',type=Path,required=True);a=p.parse_args()
    wire.PORT_RE=re.compile(r'smk|nesynth|ota|composite|sinco|usb-midi',re.I)
    dev=wire.Updater(wire.MidoBackend()).find(lambda i:i.text=='SMK-37_964')
    if not dev:raise RuntimeError('Diagnostic missing')
    try:first=read_factory(dev.link);second=read_factory(dev.link)
    finally:dev.link.close()
    if len(first)!=8192 or first!=second:raise RuntimeError('Paired factory reads do not match')
    a.output_dir.mkdir(parents=True,exist_ok=True)
    for i,data in enumerate([first,second],1):(a.output_dir/f'factory-read-{i}.bin').write_bytes(data)
    result={'identity':'SMK-37_964','read_only':True,'radio_started':False,'offset':'0xc0000','bytes':len(first),'paired_reads_match':True,'sha256':hashlib.sha256(first).hexdigest()}
    (a.output_dir/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
