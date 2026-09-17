"""Bounded large-fragment extraction search using ordinary rocket aiming."""
import argparse,json,math,os,struct,subprocess
from pathlib import Path
from replay_authored_post import pitch_for,pitch_commands
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--heights',type=float,nargs='+',default=[-1.4,-1.2,-1.0])
parser.add_argument('--trace',action='store_true',help='Record process-local rocket impact and rejection telemetry')
parser.add_argument('--output',type=Path,default=ROOT/'artifacts/larger-rubble-base-search')
args=parser.parse_args()
if not all(math.isfinite(height) for height in args.heights):parser.error('heights must be finite')
folder=args.output;folder.mkdir(parents=True,exist_ok=True)
report_path=folder/'report.json';report_path.write_text('[]\n',encoding='utf-8')
source=(ROOT/'artifacts/geomod-postedit-re/detached-live-verified/control.bin').read_bytes()
assert source[:8]==b'RFI6'+struct.pack('<I',48) and len(source)>=8+350*48
eye=json.loads((ROOT/'artifacts/authored-post-live/post-recipe.json').read_text(encoding='utf-8'))['eye']
env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))};env.update(RF_REPLAY_LEVEL='ctf06.rfl',RF_REPLAY_ARCHIVE='levelsm.vpp',RF_REPLAY_DEV_ROOM='1',RF_REPLAY_PLAYER_CHECKPOINT='1')
if args.trace:env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='230')
rows=[]
for index,height in enumerate(args.heights):
 path=folder/str(index);data=bytearray(source[:8+350*48]+bytes(250*48))
 commands,_=pitch_commands(0,pitch_for(eye,[-4.699,height,2.5]))
 for j,value in enumerate(commands):struct.pack_into('<f',data,8+48*(190+j)+12,value)
 path.with_suffix('.bin').write_bytes(data);path.with_suffix('.rfcp').unlink(missing_ok=True)
 with path.with_suffix('.log').open('wb') as log:
  result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay',str(ROOT/'Installed_Game'),str(path.with_suffix('.bin')),str(path.with_suffix('.ppm'))],cwd=ROOT,env=dict(env,RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix('.rfcp'))),stdout=log,stderr=subprocess.STDOUT,timeout=180)
 lines=path.with_suffix('.log').read_text(encoding='utf-8',errors='replace').splitlines()
 def values(label):
  line=next((line for line in reversed(lines) if line.startswith(label+' ')),None)
  return list(map(int,line.split()[1:])) if line else None
 geomod=values('GEOMOD');rockets=values('ROCKETS')
 row=dict(case=index,height=height,exit=result.returncode,radii=[],geomod=geomod,rockets=rockets,
          trace=[line for line in lines if line.startswith(('ROCKET_IMPACT ','GEOMOD_REJECT ','DETACHED_CUT_REJECT ','GEOMOD_PUBLICATION_REJECT '))])
 if geomod:
  status=geomod[5];row['edit_status']=status if status<0x80000000 else status-0x100000000
  row['committed_cuts']=geomod[7]
 if result.returncode==0:
  checkpoint=path.with_suffix('.rfcp').read_bytes();offset=checkpoint.rfind(b'RFPB')
  if offset>=0:
   version,size,count=struct.unpack_from('<3I',checkpoint,offset+4);assert version==2 and size==16+328*count and offset+size==len(checkpoint)
   row['radii']=[struct.unpack_from('<f',checkpoint,offset+16+328*j+256)[0] for j in range(count)]
 row['large_fragment']=any(radius>1 for radius in row['radii'])
 if result.returncode:row['outcome']='process_failed'
 elif geomod is None or rockets is None:row['outcome']='missing_telemetry'
 elif not row['committed_cuts']:row['outcome']='edit_rejected' if row['edit_status'] else 'no_edit'
 elif not row['radii']:row['outcome']='cut_without_retained_fragment'
 else:row['outcome']='large_fragment' if row['large_fragment'] else 'small_fragment'
 rows.append(row);print(row,flush=True)
 report_path.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
