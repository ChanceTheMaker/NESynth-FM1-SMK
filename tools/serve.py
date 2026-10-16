"""Loopback preview with an SMK USB installer; no ROM upload endpoint."""
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from functools import partial
from pathlib import Path
import argparse,hashlib,json
from urllib.parse import urlsplit
from native_smk import Bridge
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8816)
p.add_argument('--owner-firmware',action='store_true',help='Use the verified local SMK Pro core on loopback without copying private calibration into web/')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'web'
owner_app=None
if a.owner_firmware:
    build=root.parent/'build/bluetooth-loader'
    report=json.loads((build/'report.json').read_text())
    owner_app=(build/'SMK969-BLUETOOTH-RAW.bin').read_bytes()
    assert report['private_device_calibration'] and report['product']=='SMK-37_969'
    assert len(owner_app)==report['app_bytes']<=216800 and hashlib.sha256(owner_app).hexdigest()==report['sha256']
bridge=Bridge(owner=owner_app is not None)
class Handler(SimpleHTTPRequestHandler):
    def api_allowed(self,write=False):
        hosts=(f'127.0.0.1:{a.port}',f'localhost:{a.port}')
        origins=tuple('http://'+host for host in hosts)
        return self.headers.get('Host') in hosts and self.headers.get('Origin') in (origins if write else (None,*origins))
    def json_reply(self,value,status=200):
        data=json.dumps(value).encode();self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    def do_POST(self):
        if urlsplit(self.path).path!='/api/smk/jobs':self.send_error(404);return
        if not self.api_allowed(write=True):self.send_error(403);return
        try:
            size=int(self.headers.get('Content-Length','0'))
            if not 0<size<=512 or self.headers.get('Content-Type','').split(';')[0]!='application/json':raise ValueError('Invalid request')
            request=json.loads(self.rfile.read(size))
            if set(request)!={'action','model'} or request['model']!='smk-pro':raise ValueError('Unsupported local device')
            self.json_reply({'id':bridge.start(request['action'])},202)
        except (ValueError,KeyError,TypeError) as exc:self.json_reply({'error':str(exc)},400)
    def do_GET(self):
        path=urlsplit(self.path).path
        if path.startswith('/api/smk/'):
            if not self.api_allowed():self.send_error(403);return
            if path=='/api/smk/capabilities':self.json_reply({'available':bridge.available,'models':['smk-pro'] if bridge.available else []});return
            if path.startswith('/api/smk/jobs/'):
                job=bridge.status(path.removeprefix('/api/smk/jobs/'))
                self.json_reply(job if job else {'error':'Unknown operation'},200 if job else 404);return
            self.send_error(404);return
        if owner_app is not None and path in ('/firmware/nesynth.json','/firmware/nesynth-smk-pro-local.bin'):
            # Private build is loopback-only, never a deployable website asset.
            if self.headers.get('Host') not in (f'127.0.0.1:{a.port}',f'localhost:{a.port}') or self.headers.get('Origin') not in (None,f'http://127.0.0.1:{a.port}',f'http://localhost:{a.port}'):
                self.send_error(403);return
            if path.endswith('.json'):
                manifest=json.loads((root/'firmware/nesynth.json').read_text())
                manifest['models']['smk-pro']={'product':'SMK-37_969','file':'firmware/nesynth-smk-pro-local.bin','bytes':len(owner_app),'sha256':report['sha256']}
                data=json.dumps(manifest).encode();mime='application/json'
            else:data=owner_app;mime='application/octet-stream'
            self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        super().do_GET()
    def end_headers(self):
        self.send_header('Cache-Control','no-store')
        super().end_headers()
print(f'NESynth preview: http://127.0.0.1:{a.port}/',flush=True)
ThreadingHTTPServer(('127.0.0.1',a.port),partial(Handler,directory=str(root))).serve_forever()
