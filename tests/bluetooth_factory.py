import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from backup_bluetooth_factory import PREFIX,decode
class FactoryPacket(unittest.TestCase):
    def setUp(self):
        self.raw=bytes((i*9)&255 for i in range(64))
        self.packet=PREFIX+bytes([127,0])+bytes(x for b in self.raw for x in [b&127,b>>7])+b'\xf7'
    def test_roundtrip(self):self.assertEqual(decode(self.packet,127),(0,self.raw))
    def test_errors(self):
        for status in [1,2]:self.assertEqual(decode(PREFIX+bytes([127,status,0xf7]),127),(status,None))
    def test_malformed(self):
        p=self.packet
        for bad in [p[:-1],p+b'\0',p[:8]+b'\2'+p[9:],PREFIX+bytes([127,3,0xf7]),PREFIX+bytes([126,1,0xf7])]:
            with self.subTest(packet=bad),self.assertRaises(ValueError):decode(bad,127)
if __name__=='__main__':unittest.main()
