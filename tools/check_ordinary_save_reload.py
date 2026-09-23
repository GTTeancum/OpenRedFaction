"""Fresh-process ordinary load/resave and movement continuation; no host input."""
import json, math, os, struct, subprocess
from pathlib import Path
from check_ordinary_save_readiness import ROOT, SECTION_NAMES, inspect_slot

def run(folder, name, frames, source=None, output=None):
    inputs=folder/(name+'.bin');inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+b''.join(frames))
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl',RF_REPLAY_ARCHIVE='levels1.vpp',RF_REPLAY_WORLD_CHECKPOINT_PROBE='1')
    if source:env['RF_REPLAY_WORLD_SNAPSHOT_IN']=str(source)
    if output:
        env['RF_REPLAY_WORLD_SNAPSHOT_OUT']=str(output)
        for i in range(2):Path(str(output)+'.'+str(i)).unlink(missing_ok=True)
    log=folder/(name+'.log')
    with log.open('wb') as f:
        p=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(inputs),str(folder/(name+'.ppm'))],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
    lines=[l for l in log.read_text(errors='replace').splitlines() if l.startswith(('WORLD_SNAPSHOT_LOAD','WORLD_SNAPSHOT_REJECT','WORLD_SNAPSHOT_STORED'))]
    return dict(exit_code=p.returncode,log=str(log),messages=lines)

def payload(base):
    p=Path(str(base)+'.0');inspect_slot(p);return p.read_bytes()[24:]

def sections(data):
    count=struct.unpack_from('<I',data,16)[0];out={}
    for i in range(count):
        kind,offset,size=struct.unpack_from('<3I',data,128+i*12);out[SECTION_NAMES[kind-1]]=data[offset:offset+size]
    return out

def main():
    folder=ROOT/'artifacts/ordinary-save-reload';folder.mkdir(parents=True,exist_ok=True)
    neutral=struct.pack('<5f7I',*([0]*12));source=folder/'source';resaved=folder/'resaved'
    result={'reload_verified':False,'movement_verified':False,'fire_verified':False,'visual_content_verified':False,'native_verified':False}
    result['capture']=run(folder,'capture',[neutral]*120,output=source)
    if result['capture']['exit_code']==0:
        result['reload']=run(folder,'reload',[neutral],source=source,output=resaved)
        if result['reload']['exit_code']==0:
            a,b=sections(payload(source)),sections(payload(resaved));result['component_equal']={k:a[k]==b[k] for k in a}
            result['reload_verified']=all(result['component_equal'].values()) and any(l.startswith('WORLD_SNAPSHOT_LOADED ') for l in result['reload']['messages'])
            # RFI6 first float is forward movement; a short walk followed by settling.
            forward=struct.pack('<5f7I',1,0,0,0,0,*([0]*7));walked=folder/'walked'
            result['continuation']=run(folder,'walk',[neutral]+[forward]*30+[neutral]*89,source=source,output=walked)
            if result['continuation']['exit_code']==0:
                c=sections(payload(walked));result['player_before']=list(struct.unpack_from('<3f',a['player'],32));result['player_after']=list(struct.unpack_from('<3f',c['player'],32))
                result['movement_verified']=all(math.isfinite(v) for v in result['player_before']+result['player_after']) and sum((a-b)**2 for a,b in zip(result['player_before'],result['player_after']))>.01
    (folder/'report.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');print(json.dumps(result,indent=2,allow_nan=False))
    return 0 if result['reload_verified'] and result['movement_verified'] else 1
if __name__=='__main__':raise SystemExit(main())
