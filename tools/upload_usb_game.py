"""Upload a cartridge to the same FM-1/SMK library over USB MIDI; no firmware flash."""
import argparse,json,os,re,secrets,struct,subprocess,sys,time,zlib
from pathlib import Path
from cartridge import read_cartridge
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire
PREFIX=b'\xf0\x7dNU'
def packet(sequence,kind,data=b''):
    if not 0<=sequence<16384 or kind not in (0,1) or len(data)>244 or (not kind and data):raise ValueError('Invalid USB request')
    return PREFIX+bytes([sequence&127,sequence>>7,kind])+bytes(v for b in data for v in (b&127,b>>7))+b'\xf7'
def reply(data,sequence):
    if not data or len(data)<8 or data[:4]!=PREFIX or data[4:6]!=bytes([sequence&127,sequence>>7]):return None
    if len(data)>96 or data[-1]!=0xf7 or (len(data)-8)%2 or any(b>127 for b in data[1:-1]):raise ValueError('Malformed USB reply')
    if data[6]:raise RuntimeError(f'Device rejected USB library command (status {data[6]})')
    if any(data[i]>1 for i in range(8,len(data)-1,2)):raise ValueError('Malformed USB byte')
    return bytes(data[i]|data[i+1]<<7 for i in range(7,len(data)-1,2))
class Channel:
    def __init__(self,link):self.link=link;self.sequence=0
    def call(self,kind,data=b''):
        self.sequence=(self.sequence+1)&16383;self.link.drain()
        if not self.link.send(packet(self.sequence,kind,data)):raise RuntimeError('USB disconnected')
        end=time.monotonic()+3
        while time.monotonic()<end:
            value=reply(self.link.read(.2),self.sequence)
            if value is not None:return value
        raise RuntimeError('USB cartridge command timed out; reconnect and retry')
    def write(self,data):return self.call(1,data)
    def read(self):return self.call(0)
def status(link):
    prefix=b'\xf0\x7dNL\x00';link.drain();link.send(prefix+b'\xf7');end=time.monotonic()+3
    while time.monotonic()<end:
        p=link.read(.2)
        if p and p.startswith(prefix) and len(p)==36:return [sum(p[5+i*5+j]<<(7*j) for j in range(5)) for i in range(6)]
    raise RuntimeError('Device did not answer USB mode query')
def library(c):
    c.write(b'\x06');data=c.read()
    if len(data)!=40 or data[:4]!=b'NPL2':raise RuntimeError('Invalid library response')
    v=struct.unpack('<10I',data)
    if v[2]>32:raise RuntimeError('Invalid game count')
    result=dict(zip(['count','free','total','largest','game_limit','slots','generation'],v[2:9]));result['games']=[]
    for i in range(v[2]):
        c.write(b'\x07'+struct.pack('<I',i));data=c.read()
        if len(data)!=44:raise RuntimeError('Invalid game entry')
        off,n,crc=struct.unpack_from('<3I',data)
        result['games'].append({'index':i,'name':data[12:].split(b'\0')[0].decode('ascii','replace'),'bytes':n,'crc32':f'{crc:08x}'})
    c.write(b'\x00');return result
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path,nargs='?');p.add_argument('--rom-entry');p.add_argument('--prepare',action='store_true',help=argparse.SUPPRESS);a=p.parse_args()
    if not a.prepare:
        subprocess.run([sys.executable,__file__,'--prepare'],check=True);time.sleep(1)
    wire.PORT_RE=re.compile(r'fm-?1|smk|nesynth|ota|composite|sinco|usb-midi',re.I)
    up=wire.Updater(wire.MidoBackend());dev=up.wait_for(lambda i:i.text in ('SMK-37_968','SMK-37_969','FM-1_969'),20)
    if not dev:raise RuntimeError('NESynth SMK968/969 or FM1969 not found over USB')
    link=dev.link
    try:
        state=status(link)
        if a.prepare:
            if state[0]!=4:
                print('Opening USB game loader (radio off).',flush=True)
                if not link.send(b'\xf0\x7dNL\x04\xf7'):raise RuntimeError('USB handoff failed')
                os._exit(0) # Avoid closing a Windows MIDI port during device reset.
            return
        if state[0]!=4 or state[5]!=0:raise RuntimeError('Expected USB loading with radio off')
        c=Channel(link);info=library(c);print(json.dumps({'transport':'USB','radio_state':state[5],'library':info}),flush=True)
        if not a.rom:return
        rom=read_cartridge(a.rom,a.rom_entry);crc=zlib.crc32(rom)
        if not 16<=len(rom)<=262672:raise ValueError('Cartridge exceeds staging capacity')
        duplicate=any(g['bytes']==len(rom) and int(g['crc32'],16)==crc for g in info['games'])
        if not duplicate and (len(rom)>info['largest'] or info['count']>=info['slots']):raise ValueError('Not enough contiguous library space')
        started=time.monotonic();session=secrets.randbits(32) or 1;c.write(b'\x04');c.write(b'\x05'+(a.rom.stem.encode('ascii','replace')[:31] or b'GAME'))
        c.write(b'\x01'+struct.pack('<3I',session,len(rom),crc));last=0
        for off in range(0,len(rom),239):
            c.write(b'\x02'+struct.pack('<I',off)+rom[off:off+239])
            if time.monotonic()-last>=2:last=time.monotonic();print(json.dumps({'received':min(off+239,len(rom)),'total':len(rom),'transport':'USB'}),flush=True)
        data=c.read()
        if len(data)!=20 or struct.unpack('<5I',data)[:4]!=(0x314c504e,1,0,len(rom)):raise RuntimeError('Incomplete USB upload')
        c.write(b'\x03'+struct.pack('<I',session));data=c.read()
        if len(data)!=20 or struct.unpack('<5I',data)[:3]!=(0x314c504e,2,0):raise RuntimeError('Cartridge verification failed')
        print(json.dumps({'verified':True,'transport':'USB','radio_state':0,'bytes':len(rom),'crc32':f'{crc:08x}','seconds':round(time.monotonic()-started,2),'status':'Restarting to save and play; verify library after restart'}),flush=True)
        os._exit(0)
    finally:link.close()
if __name__=='__main__':main()
