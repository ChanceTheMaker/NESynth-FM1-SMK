"""Read-only inventory of RF trim candidates in a private 1 MiB flash backup.

Record interpretation follows DEADACTIVE's Hortator ble_vm.c at 7e341597:
VM item 187, 68-byte payload, low-byte record CRC and full inner CRC16.
Only offsets and counts are printed; no calibration or Bluetooth address bytes.
Finding a candidate is not proof the running SDK accepts it or that RF works.
"""
import argparse,json
from pathlib import Path

def crc16(data):
    value=0
    for b in data:
        value^=b<<8
        for _ in range(8):value=((value<<1)^0x1021 if value&0x8000 else value<<1)&0xffff
    return value

def inspect(data,board):
    if board not in ('smk37','fm1'):raise ValueError('Unknown board')
    if len(data)!=1048576:raise ValueError('Expected a complete 1 MiB private backup')
    lo,hi=(0x9d000,0xc0000) if board=='smk37' else (0xe5000,0xfc000)
    found=[];distinct=set()
    for offset in range(lo,hi-71):
        if data[offset+1]!=187 or int.from_bytes(data[offset+2:offset+4],'little')!=68<<4:continue
        record=data[offset+4:offset+72];inner=int.from_bytes(record[64:68],'little')
        if not inner or inner!=crc16(record[:64]) or data[offset]!=(crc16(record)&255):continue
        found.append(hex(offset));distinct.add(bytes(record))
    return {'board':board,'read_only':True,'search_range':[hex(lo),hex(hi)],
            'valid_record_candidates':found,'distinct_records':len(distinct),
            'hardware_validated':False,'calibration_bytes_exported':False}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('backup',type=Path)
    p.add_argument('--board',choices=['smk37','fm1'],required=True)
    p.add_argument('--compare',type=Path,help='Require a second byte-identical full read')
    a=p.parse_args();data=a.backup.read_bytes()
    if a.compare and data!=a.compare.read_bytes():raise ValueError('Paired backups do not match')
    report=inspect(data,a.board);report['paired_read_match']=bool(a.compare)
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
