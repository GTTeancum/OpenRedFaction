"""Audit authored glare bitmap requirements through the existing owned decoder.

This establishes decoder suitability and residency costs, not original bitmap
cache equivalence, archive precedence, GPU state or live glare rendering.
"""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
probe=root/'build/pc/Release/rf_entity_assets_probe.exe'
raw=subprocess.check_output([str(probe),'--glare-classes',str(root/'Installed_Game/tables.vpp'),'1000000'])
status,count,retained,peak=struct.unpack_from('<4I',raw);assert not status and len(raw)==16+300*count
classes=[];textures={}
for i in range(count):
 row=raw[16+300*i:16+300*(i+1)]
 names=[row[j:j+64].split(b'\0',1)[0].decode('cp1252') for j in (0,64,128,192)]
 fields=struct.unpack_from('<I',row,296)[0];bindings={}
 for bit,(kind,name) in enumerate(zip(('corona','volumetric','reflection'),names[1:])):
  if not fields&(1<<bit):continue
  bindings[kind]=name
  textures.setdefault(name.lower(),dict(name=name,uses=[]))['uses'].append(dict(class_index=i,class_name=names[0],kind=kind))
 classes.append(dict(index=i,name=names[0],bitmaps=bindings))
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
material=root/'build/pc/Release/rf_material_probe.exe';image=root/'build/pc/Release/rf_image_probe.exe'
out=root/'artifacts/glare-bitmaps';out.mkdir(exist_ok=True);pixels=out/'frame.rgba'
checks=0
for key,record in textures.items():
 matches=[(a,e) for a in inv if 'vpp' in a for e in a['vpp']['entries'] if e['name'].lower()==key]
 record['matches']=[dict(archive=a['path'],name=e['name']) for a,e in matches]
 record['candidates']=[]
 for a,e in matches:
  archive=root/'Installed_Game'/a['path']
  with archive.open('rb') as f:f.seek(e['offset']);header=f.read(32)
  vbm=header[:4]==b'.vbm';frames=struct.unpack_from('<I',header,24)[0] if vbm else 1
  rate=struct.unpack_from('<I',header,20)[0] if vbm else 0
  def load(budget):return list(map(int,subprocess.check_output([str(material),'--particle-animation',record['name'],str(budget),str(archive)]).split()))
  info=load(64*1024*1024);candidate=dict(archive=a['path'],status=info[0],frames=frames,rate=rate);record['candidates'].append(candidate)
  if info[0]:continue
  _,owner,image_owner,actual_frames,actual_rate,resident,index=info[:7]
  assert (actual_frames,actual_rate,index)==(frames,rate,0)
  assert len(info)==7+frames*3 and load(resident)==info and load(resident-1)==[-4,owner];checks+=3
  candidate.update(resident_bytes=resident,frame_hashes=[])
  for frame in range(frames):
   width,height,checksum=info[7+3*frame:10+3*frame]
   args=[str(image),str(archive),record['name'],str(pixels),str(width*height*4)]+([str(frame)] if vbm else [])
   subprocess.check_output(args);data=pixels.read_bytes();h=2166136261
   for b in data:h=((h^b)*16777619)&0xffffffff
   assert h==checksum,(key,frame)
   candidate['frame_hashes'].append(dict(width=width,height=height,sha256=hashlib.sha256(data).hexdigest()));checks+=1
missing=[r['name'] for r in textures.values() if not r['matches']]
failed=[r['name'] for r in textures.values() if any(c['status'] for c in r['candidates'])]
# Unique-only totals deliberately avoid assuming precedence for duplicate archives.
unique=[r['candidates'][0] for r in textures.values() if len(r['candidates'])==1 and not r['candidates'][0]['status']]
report=dict(result='PASS' if not missing and not failed else 'INCOMPLETE',classes=count,unique_bitmap_names=len(textures),checks=checks,missing=missing,decoder_failures=failed,ambiguous_archives=[r['name'] for r in textures.values() if len(r['matches'])>1],unambiguous_animation_resident_bytes=sum(c['resident_bytes'] for c in unique),animated_names=[r['name'] for r in textures.values() if any(c['frames']>1 for c in r['candidates'])],class_bindings=classes,textures=list(textures.values()),scope='All authored glare bitmap names, every matching installed archive and every frame through existing PC owned animation loader. Exact/short budgets and pixel hashes versus direct decoder; archive closes before owner hashing. Not independent pixel decoding, original bitmap-cache/precedence equivalence, compiled NXDK residency or live campaign rendering. Totals exclude glare binding arrays and renderer resources.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print({k:v for k,v in report.items() if k not in ('class_bindings','textures','scope')})
