"""Installed particle bitmap binding, ownership and exact budget checks on PC."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];j=json.loads((root/'artifacts/inventory.json').read_text());tables=(root/'Installed_Game/tables.vpp').read_bytes();names=set()
for n in ('vclip.tbl','emitters.tbl'):
 e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']==n)
 names.update(re.findall(rb'(?im)^\s*\$bitmap:\s*"([^"]*)"',tables[e['offset']:e['offset']+e['size']]))
probe=root/'build/pc/Release/rf_material_probe.exe';image_probe=root/'build/pc/Release/rf_image_probe.exe';out=root/'artifacts/particle-bitmap-tests';out.mkdir(exist_ok=True);raw=out/'image.rgba'
records=[];cases=0;missing_names=[];animations=[]
for name in sorted(names):
 matches=[(a,e) for a in j['files'] if 'vpp' in a for e in a['vpp']['entries'] if e['name'].lower()==name.decode().lower()]
 if not matches:
  missing_names.append(name.decode());continue
 a,e=matches[0];path=root/'Installed_Game'/a['path']
 with path.open('rb') as f:f.seek(e['offset']);h=f.read(32)
 animated=h[:4]==b'.vbm';count=struct.unpack_from('<I',h,24)[0] if animated else 1;rate=struct.unpack_from('<I',h,20)[0] if animated else 0
 def run(frame,budget):
  global cases
  cases+=1;return list(map(int,subprocess.check_output([str(probe),'--particle',name.decode(),str(frame),str(budget),str(root/'Installed_Game/tables.vpp'),str(path)]).split()))
 for frame in sorted({0,count-1}):
  info=run(frame,1048576);assert info[0]==0,(name,info)
  _,owner,frames,fps,w,height,resident,index,checksum=info
  assert (frames,fps,index)==(count,rate,1) and resident==owner+w*height*4
  args=[str(image_probe),str(path),name.decode(),str(raw),str(w*height*4)]+([str(frame)] if animated else [])
  subprocess.check_output(args);wanted=2166136261
  for b in raw.read_bytes():wanted=((wanted^b)*16777619)&0xffffffff
  assert checksum==wanted
  assert run(frame,resident)==info
  assert run(frame,resident-1)==[-4,owner]
  records.append(dict(name=name.decode(),archive=a['path'],frame=frame,frames=count,resident=resident,hash=checksum))
 assert run(count,1048576)==[-4,owner]
 def run_animation(budget):
  global cases
  cases+=1
  return list(map(int,subprocess.check_output([str(probe),'--particle-animation',name.decode(),str(budget),str(root/'Installed_Game/tables.vpp'),str(path)]).split()))
 info=run_animation(64*1024*1024);assert info[0]==0,(name,info)
 _,animation_owner,image_owner,frames,fps,resident,index=info[:7]
 assert (frames,fps,index)==(count,rate,1) and len(info)==7+count*3
 expected=animation_owner+count*image_owner
 for frame in range(count):
  w,height,checksum=info[7+frame*3:10+frame*3]
  single=run(frame,1048576)
  assert single[0]==0 and single[4:6]==[w,height] and single[-1]==checksum,(name,frame)
  expected+=w*height*4
 assert resident==expected
 assert run_animation(resident)==info
 assert run_animation(resident-1)==[-4,animation_owner]
 animations.append(dict(name=name.decode(),frames=count,resident=resident))
missing=subprocess.check_output([str(probe),'--particle','missing_particle.tga','0','1048576',str(root/'Installed_Game/tables.vpp')]);assert missing.split()[0]==b'-3'
missing=subprocess.check_output([str(probe),'--particle-animation','missing_particle.tga','1048576',str(root/'Installed_Game/tables.vpp')]);assert missing.split()[0]==b'-3';cases+=1
report=dict(result='PASS',bitmap_names=len(names),missing_authored=missing_names,loaded_frames=len(records),cases=cases+1,scope='PC particle resource binding compared with existing image decoders; archive/name storage released before hashing, exact and one-byte-short budgets, out-of-range frames, missing bitmap and repeated close. Compiled NXDK build only for this binding; native XEMU residency and animation scheduling remain open.',records=records)
report.update(animations=animations,animation_frames=sum(a['frames'] for a in animations),largest_animation=max(animations,key=lambda a:a['resident']))
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k not in ('records','animations')})
