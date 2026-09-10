"""Extract a contiguous input prefix from QMP pacing observations; reject conflicts/gaps."""
import argparse,hashlib,json,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('report',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
report=json.loads(args.report.read_text());records={}
def ingest(ring,exclusive_limit):
 if len(ring)!=448:raise ValueError('Wrong input ring length')
 for slot in range(64):
  row=ring[slot*7:slot*7+7];frame=row[0]
  if frame%64!=slot or frame>=exclusive_limit:continue
  value=struct.pack('<6I',*row[1:])
  if frame in records and records[frame]!=value:raise ValueError(f'Conflicting non-atomic observations for tick {frame}')
  records[frame]=value
for sample in report['samples']:
 if 'input_ring' in sample:ingest(sample['input_ring'],max(0,sample['clock'][4]-1))
final=args.report.parent/'guest-memory-final.json'
if final.exists():
 snapshot=json.loads(final.read_text());symbols=snapshot['symbols']
 if symbols.get('rf_diagnostic',{}).get('words',[0,0,0])[2]&0x80000000:
  ring=symbols.get('rf_scene_player_input_frames',{}).get('words',[])
  if ring:ingest(ring,0xffffffff)
if not records:raise ValueError('No completed input records')
count=max(records)+1
if count>60000 or set(records)!=set(range(count)):raise ValueError('Input history has gaps or exceeds replay limit; no file written')
data=b''.join(records[i] for i in range(count));args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(data)
args.output.with_suffix('.json').write_text(json.dumps({'frames':count,'sha256':hashlib.sha256(data).hexdigest(),'source_report':str(args.report),'source_sha256':hashlib.sha256(args.report.read_bytes()).hexdigest(),'scope':'Contiguous consistent observed prefix; live QMP reads are non-atomic, not an input-device trace.'},indent=2))
print(count,'input records written')
