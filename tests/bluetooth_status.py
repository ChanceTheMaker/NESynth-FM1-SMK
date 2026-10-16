"""Fixed diagnostic packets must reject malformed or oversized uint32 values."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inspect_bluetooth_diagnostic import PREFIX,decode

class StatusPacket(unittest.TestCase):
    def setUp(self):
        values=[5,1,0xfffffffe,0,0,0,0xfffffffe,1,0x9d000,4,0,0,0,0]
        self.packet=PREFIX+bytes((v>>(j*7))&127 for v in values for j in range(5))+b'\xf7'
    def test_signed_errors(self):
        result=decode(self.packet)
        self.assertEqual(result['rf_result'],-2)
        self.assertEqual(result['read_result'],-2)
        self.assertEqual(result['read_offset'],0x9d000)
    def test_malformed(self):
        p=self.packet
        for bad in [p[:-1],p+b'\0',b'\0'+p[1:],p[:5]+b'\xff'+p[6:],p[:9]+b'\x7f'+p[10:]]:
            with self.subTest(packet=bad),self.assertRaises(ValueError):decode(bad)
if __name__=='__main__':unittest.main()
