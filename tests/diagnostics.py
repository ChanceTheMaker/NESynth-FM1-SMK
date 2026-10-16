"""Exercise the read-only host diagnostic protocol without accessing MIDI hardware."""
import contextlib,io,json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import diagnose_device as diagnostic

class Link:
    def __init__(self,version):self.version=version;self.sample=0;self.closed=False
    def drain(self):pass
    def send(self,data):
        assert data==bytes([0xf0,0x7d,ord('N'),ord('S'),self.version,0xf7])
        self.sample+=1
    def read(self,timeout):
        fields=diagnostic.FIELDS[:40] if self.version==3 else diagnostic.FIELDS[:]
        if self.version==5:fields+=['lcd_busy','lcd_timeouts','lcd_failed','usb_irq_ticks','clock_div','clock_sys']
        values={key:0 for key in fields}
        values.update(uptime_ms=self.sample*2000,frames=self.sample*120,display_frames=self.sample*40,
                      cpu1_jobs=self.sample*40,cpu1_online=1,emu_measured_frames=self.sample*120,
                      emu_total_ticks=self.sample*24000000,cpu_ticks=self.sample*12000000,
                      scan_irq_ticks=self.sample*240000,usb_irq_ticks=self.sample*480000,
                      clock_div=0x123,clock_sys=0x456)
        return bytes([0xf0,0x7d,ord('N'),ord('S'),self.version]+[
            (values[key]>>(7*j))&127 for key in fields for j in range(5)]+[0xf7])
    def close(self):self.closed=True

class Diagnostics(unittest.TestCase):
    def test_versions_and_rates(self):
        for board,version in [('fm1',3),('fm1',4),('smk37',5)]:
            with self.subTest(board=board,version=version),tempfile.TemporaryDirectory() as folder:
                link=Link(version);model='SMK-37' if board=='smk37' else 'FM-1'
                device=SimpleNamespace(link=link,id=SimpleNamespace(model=model,text=model+'_962'))
                def find(want):
                    self.assertTrue(want(device.id))
                    self.assertFalse(want(SimpleNamespace(model='unrelated')))
                    return device
                updater=SimpleNamespace(find=find)
                output=Path(folder)/'new'/'report.json'
                args=['diagnose','--board',board,'--version',str(version),'--samples','2','--interval','0','--output',str(output)]
                with patch.object(sys,'argv',args),patch.object(diagnostic.wire,'MidoBackend'),patch.object(diagnostic.wire,'Updater',return_value=updater),patch.object(diagnostic.wire,'PORT_RE',diagnostic.wire.PORT_RE),contextlib.redirect_stdout(io.StringIO()):
                    diagnostic.main()
                result=json.loads(output.read_text())
                self.assertTrue(link.closed)
                self.assertEqual(result['rates']['emulation_fps'],60)
                self.assertEqual(result['rates']['display_fps'],20)
                self.assertAlmostEqual(result['mean_frame_ms'],1000/120)
                if version>=4:self.assertAlmostEqual(result['profile_ms_per_frame']['scan_irq_ticks'],10/120)
                if version==5:
                    self.assertAlmostEqual(result['profile_ms_per_frame']['usb_irq_ticks'],20/120)
                    self.assertEqual(result['samples'][0]['clock_div'],0x123)

if __name__=='__main__':unittest.main()
