import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_ble_calibration import inspect,crc16

class Calibration(unittest.TestCase):
    def test_crc_vector(self):self.assertEqual(crc16(b'123456789'),0x31c3)
    def test_bounds_crc_and_board_ranges(self):
        data=bytearray(b'\xff'*1048576);payload=bytes(range(64))
        record=payload+crc16(payload).to_bytes(4,'little')
        entry=bytes([crc16(record)&255,187])+(68<<4).to_bytes(2,'little')+record
        for at in [0x9d000,0xc0000-72,0xe5000]:data[at:at+72]=entry
        self.assertEqual(inspect(data,'smk37')['valid_record_candidates'],['0x9d000',hex(0xc0000-72)])
        self.assertEqual(inspect(data,'fm1')['valid_record_candidates'],['0xe5000'])
        self.assertEqual(inspect(data,'smk37')['distinct_records'],1)
        data[0x9d005]^=1
        self.assertEqual(len(inspect(data,'smk37')['valid_record_candidates']),1)
        data[0xc0000-72]^=1
        self.assertEqual(inspect(data,'smk37')['valid_record_candidates'],[])
        with self.assertRaises(ValueError):inspect(data[:-1],'smk37')
        with self.assertRaises(ValueError):inspect(data,'unknown')

if __name__=='__main__':unittest.main()
