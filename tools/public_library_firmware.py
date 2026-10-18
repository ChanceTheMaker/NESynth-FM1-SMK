"""Stage calibration-free NESynth library cores for automatic browser setup."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'web/firmware';models={}
    for folder,tag,model,board,limit in [('fm1-bluetooth-loader','FM1','fm1','fm1',204512),('public-bluetooth-loader','SMK','smk-pro','smk37',216800)]:
        revision=969 if model=='fm1' else 970
        build=ROOT/'build'/folder;report=json.loads((build/'report.json').read_text())
        data=(build/f'{tag}{revision}-BLUETOOTH-RAW.bin').read_bytes()
        assert report['private_device_calibration'] is False, 'Private calibration must never be staged for the website'
        assert report['board']==board and report['ram_reach_check'] and report['usb_updater']
        assert report['product']==('FM-1_969' if model=='fm1' else 'SMK-37_970')
        assert len(data)==report['app_bytes']<=limit and hashlib.sha256(data).hexdigest()==report['sha256']
        file=f'nesynth-{board}-{revision}.bin';(out/file).write_bytes(data)
        models[model]={'product':report['product'],'file':'firmware/'+file,'bytes':len(data),'sha256':report['sha256']}
    models['smk-elite']=dict(models['smk-pro'])
    (out/'nesynth.json').write_text(json.dumps({'version':969,'models':models},indent=2)+'\n')
    print('Staged verified, calibration-free FM-1 and SMK library cores; no games included.')
if __name__=='__main__':main()
