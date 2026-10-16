"""Connect to NESynth SMK Test and verify its RAM-only echo. Requires bleak.

This tool never installs firmware or writes cartridge/flash storage. Run only
after the local diagnostic is installed and PLAY has enabled advertising.
"""
import argparse,asyncio,json,secrets,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/bluetooth-python-deps'))
SERVICE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7e0'
VALUE='f86c4cf1-cf8b-4bc4-bc4c-d6f47eabe7e1'
NAME='NESynth SMK Test'

def decode(data):
    if len(data)!=16 or bytes(data[:4])!=b'NPB1':raise ValueError('Unexpected diagnostic response')
    return int.from_bytes(data[4:8],'little'),bytes(data[8:16])

async def test(timeout):
    try:from bleak import BleakClient,BleakScanner
    except ImportError:raise SystemExit('Install bleak in a local Python environment first: python -m pip install bleak')
    discovered=await BleakScanner.discover(timeout=timeout,return_adv=True)
    matches=[d for d,adv in discovered.values() if adv.local_name==NAME and SERVICE in [s.lower() for s in adv.service_uuids]]
    if len(matches)!=1:raise RuntimeError(f'Expected one {NAME} advertising device; found {len(matches)}')
    async with BleakClient(matches[0],timeout=timeout) as client:
        svc=client.services.get_service(SERVICE)
        if not svc or not svc.get_characteristic(VALUE):raise RuntimeError('Diagnostic GATT service missing')
        before,_=decode(await client.read_gatt_char(VALUE));nonce=secrets.token_bytes(8)
        await client.write_gatt_char(VALUE,nonce,response=True)
        count,echo=decode(await client.read_gatt_char(VALUE))
        if count!=((before+1)&0xffffffff) or echo!=nonce:raise RuntimeError('Echo or write count mismatch')
    print(json.dumps({'device':NAME,'connected':True,'echo_verified':True,'flash_writes':False,'disconnected':True},indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--timeout',type=float,default=15)
    args=parser.parse_args()
    if not 1<=args.timeout<=60:parser.error('--timeout must be between 1 and 60 seconds')
    asyncio.run(test(args.timeout))
