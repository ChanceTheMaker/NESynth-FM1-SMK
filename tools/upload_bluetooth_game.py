"""Send a local .nes or ZIP to the integrated NESynth SMK965 Bluetooth loader."""
import argparse,asyncio,json,secrets,struct,sys,time,zlib
from pathlib import Path
from cartridge import read_cartridge
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/bluetooth-python-deps'))
SERVICE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f0'
VALUE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7f1'

def decode(data):
    if len(data)!=20 or data[:4]!=b'NPL1':raise ValueError('Unexpected loader response')
    return struct.unpack('<4I',data[4:])

async def upload(args):
    from bleak import BleakClient,BleakScanner
    rom=read_cartridge(args.rom,args.rom_entry)
    if not 16<=len(rom)<=262672 or rom[:4]!=b'NES\x1a':raise ValueError('Expected an iNES cartridge no larger than 262672 bytes')
    found=await BleakScanner.discover(timeout=args.timeout,return_adv=True)
    matches=[d for d,a in found.values() if SERVICE in [s.lower() for s in a.service_uuids]]
    if len(matches)!=1:raise RuntimeError(f'Expected one NESynth SMK loader; found {len(matches)}. Hold PLAY + REC for two seconds to open it.')
    session=secrets.randbits(32) or 1;crc=zlib.crc32(rom);started=time.monotonic()
    async with BleakClient(matches[0],timeout=args.timeout) as client:
        state,error,received,capacity=decode(await client.read_gatt_char(VALUE))
        if len(rom)>capacity:raise ValueError('Cartridge exceeds device capacity')
        await client.write_gatt_char(VALUE,b'\x04',response=True)
        await client.write_gatt_char(VALUE,b'\x01'+struct.pack('<3I',session,len(rom),crc),response=True)
        chunk=min(239,max(15,client.mtu_size-8)) if not args.slow else 15
        last=0
        for off in range(0,len(rom),chunk):
            await client.write_gatt_char(VALUE,b'\x02'+struct.pack('<I',off)+rom[off:off+chunk],response=True)
            if time.monotonic()-last>=3:
                last=time.monotonic();print(json.dumps({'sent':min(off+chunk,len(rom)),'total':len(rom),'chunk':chunk}),flush=True)
        state,error,received,_=decode(await client.read_gatt_char(VALUE))
        if state!=1 or error or received!=len(rom):raise RuntimeError(f'Transfer incomplete: {state,error,received}')
        await client.write_gatt_char(VALUE,b'\x03'+struct.pack('<I',session),response=True)
        state,error,received,_=decode(await client.read_gatt_char(VALUE))
        if state!=2 or error:raise RuntimeError(f'Cartridge was not verified: {state,error}')
        print(json.dumps({'verified':True,'bytes':len(rom),'crc32':f'{crc:08x}','seconds':round(time.monotonic()-started,1),'status':'SMK restarting to install and play; verify USB status afterward'}),flush=True)
        # The device intentionally resets after the final ACK; do not treat it
        # as proof of persistent installation. USB/gameplay verification is separate.

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--rom-entry');p.add_argument('--timeout',type=float,default=12);p.add_argument('--slow',action='store_true');a=p.parse_args();asyncio.run(upload(a))
