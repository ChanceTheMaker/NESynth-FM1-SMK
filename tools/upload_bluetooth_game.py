"""Send a local .nes or ZIP to the integrated NESynth FM-1/SMK Bluetooth loader."""
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
    if len(matches)!=1:raise RuntimeError(f'Expected one NESynth loader; found {len(matches)}. Hold PLAY + REC for the library, then press PLAY (Select) for Bluetooth.')
    session=secrets.randbits(32) or 1;crc=zlib.crc32(rom);started=time.monotonic()
    async with BleakClient(matches[0],timeout=max(30,args.timeout)) as client:
        state,error,received,capacity=decode(await client.read_gatt_char(VALUE))
        if len(rom)>capacity:raise ValueError('Cartridge exceeds device capacity')
        await client.write_gatt_char(VALUE,b'\x04',response=True)
        try:
            await client.write_gatt_char(VALUE,b'\x06',response=True)
        except Exception:
            if not client.is_connected:raise
        else:
            info=await client.read_gatt_char(VALUE)
            if len(info)!=40 or info[:4]!=b'NPL2':raise RuntimeError('Invalid library response')
            await client.write_gatt_char(VALUE,b'\x00',response=True)
            name=args.rom.stem.encode('ascii','replace')[:31] or b'GAME'
            await client.write_gatt_char(VALUE,b'\x05'+name,response=True)
        await client.write_gatt_char(VALUE,b'\x01'+struct.pack('<3I',session,len(rom),crc),response=True)
        characteristic=client.services.get_characteristic(VALUE)
        burst='write-without-response' in characteristic.properties and not args.acknowledged
        maximum=characteristic.max_write_without_response_size if burst else client.mtu_size-3
        chunk=min(239,max(1,maximum-5)) if not args.slow else 15
        last=0;off=0;transfer_started=time.monotonic();retries_total=0
        while off<len(rom):
            end=min(len(rom),off+chunk*(8 if burst else 1));retries=0
            while True:
                for pos in range(off,end,chunk):
                    await client.write_gatt_char(VALUE,b'\x02'+struct.pack('<I',pos)+rom[pos:min(pos+chunk,end)],response=not burst)
                if not burst:off=end;break
                state,error,received,_=decode(await client.read_gatt_char(VALUE))
                if state!=1 or error not in (0,2) or not off<=received<=end:
                    raise RuntimeError(f'Invalid cumulative acknowledgment: {state,error,received,off,end}')
                off=received
                if off==end:
                    if error:raise RuntimeError(f'Transfer error: {error}')
                    break
                retries+=1;retries_total+=1
                if retries>3:raise RuntimeError('Bluetooth transfer stalled; move the device closer and retry')
            if time.monotonic()-last>=3:
                last=time.monotonic();print(json.dumps({'received':off,'total':len(rom),'chunk':chunk,'burst':burst}),flush=True)
        data_seconds=time.monotonic()-transfer_started
        state,error,received,_=decode(await client.read_gatt_char(VALUE))
        if state!=1 or error or received!=len(rom):raise RuntimeError(f'Transfer incomplete: {state,error,received}')
        await client.write_gatt_char(VALUE,b'\x03'+struct.pack('<I',session),response=True)
        state,error,received,_=decode(await client.read_gatt_char(VALUE))
        if state!=2 or error:raise RuntimeError(f'Cartridge was not verified: {state,error}')
        print(json.dumps({'verified':True,'bytes':len(rom),'crc32':f'{crc:08x}','seconds':round(time.monotonic()-started,2),'data_seconds':round(data_seconds,2),'burst':burst,'retries':retries_total,'status':'SMK restarting to install and play; verify USB status afterward'}),flush=True)
        # The device intentionally resets after the final ACK; do not treat it
        # as proof of persistent installation. USB/gameplay verification is separate.

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--rom-entry');p.add_argument('--timeout',type=float,default=12);p.add_argument('--slow',action='store_true');p.add_argument('--acknowledged',action='store_true',help='Use legacy per-packet ATT responses for comparison');a=p.parse_args();asyncio.run(upload(a))
