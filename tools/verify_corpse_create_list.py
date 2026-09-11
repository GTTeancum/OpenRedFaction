"""Full original/PC/NXDK constructor with existing corpse retention lists."""
import json,random,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=runpy.run_path(str(root/'tools/verify_corpse_create.py'))
base['cases'].clear();base['expected'].clear();base['traces'].clear();before=[];after=[];mapped=False;fades=0;new_fades=0
w=base['w'];node_base=0x30010000
def prepare(g):
 global mapped
 u=g['u'];put=g['put'];case=g['case'];count=case%30;rng=random.Random(0x416da3+case)
 if not mapped:u.mem_map(node_base,65536);mapped=True
 specs=[];mode=(case//30)%3
 previous=0x5cabb8
 for j in range(count):
  node=node_base+j*0x318;flags=rng.choice((0,0,0x4000)) if mode==2 else 0
  bits=rng.choice((0,0,0,1,2,0x40,4)) if mode==2 else 0
  created=1000 if mode==0 else 1001+rng.randrange(4) if mode==1 else 999+rng.randrange(3)
  created_bits=struct.unpack('<I',struct.pack('<f',created))[0];fade=0x3e800000
  specs.append((flags,bits,created_bits,fade));u.mem_write(node,bytes(0x318));put(node+0x7c,flags);put(node+0x29c,bits);put(node+0x294,created_bits);put(node+0x298,fade)
  put(node+0x290,previous);put(node+0x28c,0x5cabb8);put(previous+0x28c,node);put(0x5cae48,node);previous=node
 put(0x5caed0,count);before.append(specs)
def observe(g):
 global fades,new_fades
 base['observe'](g);specs=before[-1];read=g['read'];tail=[]
 for j,spec in enumerate(specs):
  flags,fade=read(node_base+j*0x318+0x29c),read(node_base+j*0x318+0x298);tail.extend((flags,fade))
  if not spec[1]&1 and flags&1:fades+=1
 tail.append(read(g['corpse']+0x298) if g['result'] else 0)
 if g['result'] and read(g['corpse']+0x29c)&1:new_fades+=1
 after.append(w(*tail))
runpy.run_path(str(root/'tools/verify_corpse_create_original.py'),init_globals={'prepare_list':prepare,'observe_case':observe,'write_report':False})
inputs=[]
for raw,specs in zip(base['cases'],before):inputs.append(raw[:92]+w(len(specs))+b''.join(w(*s) for s in specs)+raw[92:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-create-list'],input=b''.join(inputs))
offset=0
for i,(raw,want,specs,tail) in enumerate(zip(base['cases'],base['expected'],before,after)):
 full=want+tail;got=actual[offset:offset+len(full)];offset+=len(full)
 assert got==full,('PC',i,got.hex(),full.hex())
 assert base['native_case'](i,raw,want,specs)==tail,('NXDK retention',i)
assert offset==len(actual)
report=dict(result='PASS',cases=len(before),existing_bodies=sum(map(len,before)),existing_fades=fades,new_body_fades=new_fades,max_existing=max(map(len,before)),scope='Full original416940 and shared PC/NXDK creation with0..29 existing corpses. Equal timestamps, newer existing bodies, mixed protected/fading flags, allocation failure and null sources. Resource callbacks supplied; no native XEMU or live resource integration.')
(root/'artifacts/corpse-create-list-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
