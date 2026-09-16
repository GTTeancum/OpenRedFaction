"""Ordinary headless PC replay of admitted-but-capacity-rejected destruction.
No builds, desktop input or source mutation. Three sequential process runs.
Requires built build/pc/Release/rf_pc_play.exe and local Installed_Game assets.
The ninth and next rocket remain admissions despite the8-cut limit; compare
restored vs uninterrupted RFDS, physical geometry and retained atlas exactly.
"""
import os,struct,json,subprocess,hashlib,time
from datetime import datetime
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/geomod-save-contract/failed-admission'/datetime.now().strftime("%Y%m%d-%H%M%S-%f");OUT.mkdir(parents=True)
# Same ordinary input as the retained eight-cut replay, generated locally.
base=b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,0,0,.7 if i<90 else 0,0,0,0,0,0,int(i in (10,20,30,40)),0) for i in range(1500))
exe=ROOT/'build/pc/Release/rf_pc_play.exe';exe_hash=hashlib.sha256(exe.read_bytes()).hexdigest();(OUT/'binary.sha256').write_text(exe_hash)
def run(name,shots,incoming=None):
 folder=OUT/name;folder.mkdir(exist_ok=False);data=bytearray(base)
 for frame in range((len(data)-8)//48):struct.pack_into('<I',data,8+frame*48+32,int(frame in shots))
 (folder/'input.bin').write_bytes(data)
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_TRACE='1',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(folder/'state.rfds'),RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(folder/'physical.mesh'),RF_REPLAY_TERRAIN_BASE_AUDIT=str(folder/'atlas.csv'))
 if incoming:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(incoming)
 start=time.monotonic();r=subprocess.run([str(exe),'--dev-room-replay',str(ROOT/'Installed_Game'),str(folder/'input.bin'),str(folder/'frame.ppm')],cwd=ROOT,env=env,capture_output=True,text=True,timeout=240)
 log=r.stdout+r.stderr;(folder/'run.log').write_text(log);r.check_returncode();b=(folder/'state.rfds').read_bytes()
 row=dict(name=name,admissions=struct.unpack_from('<I',b,240)[0],cuts=struct.unpack_from('<I',b,300)[0],orientation_rng=struct.unpack_from('<I',b,244)[0],noise_rng=struct.unpack_from('<I',b,256)[0],seconds=time.monotonic()-start,lines=[l for l in log.splitlines() if l.startswith(('GEOMOD ','ROCKETS ','GEOMOD_ADMISSION ','GEOMOD_CHECKPOINT_'))])
 (folder/'summary.json').write_text(json.dumps(row,indent=2));print(json.dumps(row),flush=True);return folder,row
shots=[110,220,330,440,550,660,770,880,990]
a,ar=run('after-ninth',shots)
assert ar['admissions']>ar['cuts'] and ar['cuts']==8,(ar['admissions'],ar['cuts'])
b,br=run('uninterrupted-next',shots+[1210])
c,cr=run('restored-next',[1210],a/'state.rfds')
checks={f:(b/f).read_bytes()==(c/f).read_bytes() for f in ['state.rfds','physical.mesh','atlas.csv']}
assert all(checks.values()),checks
assert br['admissions']==ar['admissions']+1 and cr['admissions']==br['admissions'] and br['orientation_rng']==cr['orientation_rng'],'Next admission/RNG does not match after restore'
assert exe_hash==hashlib.sha256(exe.read_bytes()).hexdigest()
report=dict(result='PASS',root=str(ROOT),binary=str(exe),binary_sha256=exe_hash,input_template_sha256=hashlib.sha256(base).hexdigest(),states=[ar,br,cr],exact=checks,scope='Ordinary PC rocket input. Eight committed limit forces ninth admission CSG rejection; next admitted-but-rejected impact compares restored against uninterrupted. No visual/complete-game save claim.')
(OUT/'report.json').write_text(json.dumps(report,indent=2));print('PASS failed admission save/reload and next blast',flush=True)
