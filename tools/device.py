"""NESynth FM-1 trial: verify exact package and starting identity before writing."""
import argparse,hashlib,json,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'build/python-deps'),str(ROOT/'vendor/felucca/tools')]
import fm1_install as wire

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['info','install','restore'])
    p.add_argument('--trial',type=Path,default=ROOT/'build/fm1/trial-diagnostic.json')
    p.add_argument('--expected-start');p.add_argument('--sha256');p.add_argument('--yes',action='store_true');a=p.parse_args()
    if a.action!='info' and not(a.yes and a.expected_start and a.sha256):p.error('Writing requires --yes, --expected-start and the reviewed --sha256')
    report=json.loads(a.trial.read_text()) if a.action!='info' else None
    if report:
        path=Path(report['restore_package']) if a.action=='restore' else a.trial.parent/report['package']
        expected=report['restore_sha256'] if a.action=='restore' else report['sha256']
        if hashlib.sha256(path.read_bytes()).hexdigest()!=expected or expected!=a.sha256:raise RuntimeError('Reviewed package hash mismatch')
        if not all(x in report['validation'] for x in ['OTA simulated flash tests','loader simulated flash tests']):raise RuntimeError('Missing package validation')
        product,image=wire.load_package(path,False)
        if wire.model_of(product)!='FM-1':raise RuntimeError('FM-1 packages only; SMK flashing is not enabled')
    backend=wire.MidoBackend()
    if len(backend.input_names())!=1:raise RuntimeError('Expected exactly one MIDI input device; no write attempted')
    updater=wire.Updater(backend);dev=updater.find(lambda i:i.model in ('FM-1','ota-FM-1'))
    if not dev:raise RuntimeError('No FM-1 identity response')
    before=dev.id.text;print('Connected:',before,'port:',dev.name,flush=True)
    if a.action=='info':dev.link.close();return
    if before!=a.expected_start:dev.link.close();raise RuntimeError('Starting identity changed; refusing write')
    history=[];last=None
    def progress(phase,value=None):
        nonlocal last
        if (phase,value)==last:return
        last=(phase,value);history.append({'time':time.time(),'phase':phase,'value':value});print(phase,value,flush=True)
    evidence=ROOT/'build/fm1'/('device-'+a.action+'-'+report['cartridge']+'.json')
    try:
        after=updater.install(dev,image,product,progress)
        result={'before':before,'after':after,'sha256':expected,'usb_restart_verified':True,'events':history}
        evidence.write_text(json.dumps(result,indent=2)+'\n');print('Verified:',after,flush=True)
    except Exception as error:
        evidence.write_text(json.dumps({'before':before,'error':str(error),'events':history},indent=2)+'\n');raise
    finally:dev.link.close()

if __name__=='__main__':main()
