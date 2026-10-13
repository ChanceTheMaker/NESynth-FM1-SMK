"""Serve only browser assets on loopback; no ROM upload endpoint."""
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from functools import partial
from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=8816);a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'web'
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control','no-store')
        super().end_headers()
print(f'NESynth preview: http://127.0.0.1:{a.port}/',flush=True)
ThreadingHTTPServer(('127.0.0.1',a.port),partial(Handler,directory=str(root))).serve_forever()
