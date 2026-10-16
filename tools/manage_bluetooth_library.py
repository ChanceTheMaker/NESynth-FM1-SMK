"""List, play, or delete an SMK967 library entry over Bluetooth."""
import argparse,asyncio,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/bluetooth-python-deps'))
from upload_bluetooth_game import SERVICE,VALUE
async def main(a):
    from bleak import BleakScanner,BleakClient
    found=await BleakScanner.discover(timeout=a.timeout,return_adv=True)
    matches=[d for d,v in found.values() if SERVICE in [s.lower() for s in v.service_uuids]]
    if len(matches)!=1:raise RuntimeError('Expected one advertising NESynth SMK')
    async with BleakClient(matches[0],timeout=30) as c:
        await c.write_gatt_char(VALUE,b'\x06',response=True)
        summary=await c.read_gatt_char(VALUE)
        if len(summary)!=40 or summary[:4]!=b'NPL2':raise RuntimeError('SMK967 library required')
        v=struct.unpack('<10I',summary)
        if v[2]>32:raise RuntimeError('Invalid game count')
        result=dict(zip(['count','free','total','largest','game_limit','slots','generation'],v[2:9]));result['games']=[]
        for i in range(v[2]):
            await c.write_gatt_char(VALUE,b'\x07'+struct.pack('<I',i),response=True)
            data=await c.read_gatt_char(VALUE)
            if len(data)!=44:raise RuntimeError('Invalid game entry')
            off,n,crc=struct.unpack_from('<3I',data)
            result['games'].append({'index':i,'name':data[12:].split(b'\0')[0].decode('ascii','replace'),'bytes':n,'crc32':f'{crc:08x}'})
        await c.write_gatt_char(VALUE,b'\x00',response=True)
        print(json.dumps(result,indent=2),flush=True)
        action=a.play if a.play is not None else a.delete
        if action is not None:
            if not 0<=action<len(result['games']):raise RuntimeError('Game index out of range; nothing changed')
            game=result['games'][action]
            await c.write_gatt_char(VALUE,bytes([9 if a.play is not None else 8])+struct.pack('<II',action,int(game['crc32'],16)),response=True)
            print(json.dumps({'requested':'play' if a.play is not None else 'delete','game':game['name'],'verify_after_restart':True}),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group();g.add_argument('--play',type=int);g.add_argument('--delete',type=int);p.add_argument('--timeout',type=float,default=6);asyncio.run(main(p.parse_args()))
