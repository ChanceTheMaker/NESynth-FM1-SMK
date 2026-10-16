"""Inspect SMK965 cartridge/loader state over USB; optionally open Bluetooth loading."""
import argparse,json,os,re,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--open-loader',action='store_true');a=p.parse_args()
    wire.PORT_RE=re.compile(r'smk|nesynth|ota|composite|sinco|usb-midi',re.I)
    up=wire.Updater(wire.MidoBackend());d=up.find(lambda i:i.text=='SMK-37_965')
    if not d:raise RuntimeError('SMK-37_965 not found')
    prefix=b'\xf0\x7dNL\x00';d.link.drain();d.link.send(prefix+b'\xf7');end=time.monotonic()+4
    while time.monotonic()<end:
        packet=d.link.read(.2)
        if not packet or not packet.startswith(prefix):continue
        if len(packet)!=36 or packet[-1]!=0xf7:raise RuntimeError('Malformed loader status')
        values=[sum(packet[5+i*5+j]<<(7*j) for j in range(5)) for i in range(6)]
        result=dict(zip(['loader_mode','error','installed_bytes','installed_crc32','received_bytes','radio_state'],values));result['identity']=d.id.text
        print(json.dumps(result,indent=2),flush=True)
        if a.open_loader and not result['loader_mode']:
            d.link.send(b'\xf0\x7dNL\x01\xf7');os._exit(0)
        d.link.close();return
    d.link.close();raise RuntimeError('No loader status response')

if __name__=='__main__':main()
