"""Verify complete static render resource ownership and archive-independent data."""
import json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1]
probe=root/'build/pc/Release/rf_model_file_probe.exe'
def hash_bytes(data):
 h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 return h
records=[];unsupported=[];animated=0
for archive in json.loads((root/'artifacts/inventory.json').read_text())['files']:
 for e in archive.get('vpp',{}).get('entries',[]):
  if not e['name'].lower().endswith(('.v3m','.v3c')):continue
  path=root/'Installed_Game'/archive['path']
  def call(budget):return subprocess.check_output([str(probe),str(path),e['name'],'--static-resource',str(budget)],text=True).splitlines()
  if e['name'].lower().endswith('.v3c'):
   assert call(4*1024*1024)==['R -3 0 0 0 0'];animated+=1;continue
  with path.open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  all_sections=inspect(raw)['sections'];sections=[s for s in all_sections if s['type']=='0x5355424d'];parts=bytearray();materials=bytearray();p_lines=[];g_lines=[];li=0;bytes_owned=52+48*len(sections);bad=False
  spheres=bytearray()
  for s in all_sections:
   if s['type']=='0x43535048':
    start=s['offset']+8;assert s['declared']==44
    spheres.extend(raw[start:start+24]+bytes(4)+raw[start+24:start+44])
  bytes_owned+=len(spheres)
  for s in sections:
   count=len(s['lods']);start=s['offset']+8+56+count*4
   parts.extend(raw[start:start+40]+struct.pack('<2I',li,count));materials.extend(raw[s['material_offset']:s['material_offset']+s['materials']*84]);bytes_owned+=s['materials']*84
   for local,lod in enumerate(s['lods']):
    begin=lod['data_offset'];relative=(lod['batches']*56+15)&~15;planes=bytearray();vertices=triangles=0
    for bi in range(lod['batches']):
     v,t,pos,ix,extra,links,uv,fmt=struct.unpack_from('<7HI',raw,begin+lod['data_bytes']+4+bi*18);bad|=fmt!=0x518c41;vertices+=v;triangles+=t
     for region,size in enumerate([pos,pos,uv,ix,t*16 if lod['flags']&32 else 0,extra,links,lod['unknown']*2 if lod['flags']&1 else 0]):
      if region==4:planes.extend(raw[begin+relative:begin+relative+size])
      relative=(relative+size+15)&~15
    threshold=struct.unpack_from('<I',raw,s['offset']+64+local*4)[0]
    p_lines.append(f'P {li} {threshold} {hash_bytes(planes)}')
    g=32+lod['batches']*20+vertices*44+triangles*8
    g_lines.append(f"G {li} {lod['batches']} {vertices} {triangles} {g}")
    bytes_owned+=40+g-32+triangles*16;li+=1
  result=call(4*1024*1024)
  if bad:
   assert result==['R -2 0 0 0 0'],(e['name'],result);unsupported.append(e['name']);continue
  assert result[0]==f'R 0 {len(sections)} {li} {len(materials)//84} {bytes_owned}',(e['name'],result[0],bytes_owned)
  assert result[1]==f'H {hash_bytes(parts)} {hash_bytes(materials)}',e['name']
  assert result[2]==f'C {len(spheres)//48} {hash_bytes(spheres)}',e['name']
  assert [s for s in result if s.startswith('P ')]==p_lines,e['name']
  assert [s for s in result if s.startswith('G ')]==g_lines,e['name']
  # This existing geometry probe is independently checked against serialized
  # vertices/triangles/reuse in verify_model_vertices.py --static.
  decoded=subprocess.check_output([str(probe),str(path),e['name'],'--geometry'],text=True).splitlines()
  assert [s for s in result if s.startswith(('G ','V ','M '))]==decoded,e['name']
  assert call(bytes_owned)==result,e['name']
  assert call(bytes_owned-1)==['R -4 0 0 0 0'],e['name']
  records.append(dict(model=e['name'],parts=len(sections),lods=li,bytes=bytes_owned,spheres=len(spheres)//48))
report=dict(result='PASS',models=len(records),unsupported=unsupported,animated_rejections=animated,max_resource_bytes=max(r['bytes'] for r in records),lods=sum(r['lods'] for r in records),spheres=sum(r['spheres'] for r in records),scope='PC actual archive composition; serialized parts/material/threshold/plane and decoded CSPH hashes, geometry probe agreement, exact/short budgets, outputs retained after archive close and repeated cleanup. Lower-level geometry decoding separately archive verified; no complete original loader or native XEMU claim.',records=records)
(root/'artifacts/static-render-resource.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
