"""Check shared NPC images and material remapping against archive records and separate loads."""
import hashlib,json,struct,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
probe=root/'build/pc/Release/rf_material_probe.exe'
reference=json.loads((root/'artifacts/npc-texture-residency.json').read_text())
inventory=json.loads((root/'artifacts/inventory.json').read_text())['files']
entries=next(a for a in inventory if a['path']=='meshes.vpp')['vpp']['entries']
entries={e['name'].lower():e for e in entries}
archives=[str(game/a) for a in ('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')]
def run(args,**kwargs):
 r=subprocess.run([str(probe),*args],capture_output=True,text=True,**kwargs)
 assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
 return r.stdout.splitlines()
def name(raw):return raw.split(b'\0',1)[0].decode('ascii')
results=[]
for level in reference['levels']:
 lines=run(['--entities',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level['level'],'16777216',*archives])
 header=list(map(int,lines[0].split()[1:]));assert header[0]==level['appearances'] and header[2]==level['unique_textures']
 spans=[list(map(int,l.split()[1:])) for l in lines if l.startswith('A ')]
 records=[(bytes.fromhex(l.split()[1]),int(l.split()[2])) for l in lines if l.startswith('M ')]
 textures=[list(map(int,l.split()[1:])) for l in lines if l.startswith('T ')]
 unique={};at=0
 for binding,span in zip(level['bindings'],spans,strict=True):
  e=entries[binding['model'].lower()]
  with (game/'meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
  disk=[]
  for section in inspect(raw)['sections']:
   if section['type']=='0x5355424d':disk.extend(raw[section['material_offset']+i*84:section['material_offset']+(i+1)*84] for i in range(section['materials']))
  assert span==[binding['index'],at,at+len(disk)]
  # The owned record retains selected primary names; cross-check the ordered
  # selection against the independently measured authored appearance report.
  selected=[];overrides=[];local={};maps=[]
  for j,d in enumerate(disk):
   record=records[at+j][0];primary=name(record[0x14:0x34]);secondary=name(d[48:80]);overrides.append(primary)
   pair=[]
   for n in (primary,secondary):
    if n:
     key=n.lower();selected.append(key);unique.setdefault(key,len(unique));local.setdefault(key,len(local));pair.append((unique[key],local[key]))
    else:pair.append((-1,-1))
   maps.append(pair)
  assert selected==binding['textures'],(level['level'],binding['index'],selected,binding['textures'])
  separate=run(['--model-skin',str(game/'meshes.vpp'),binding['model'],'16777216',*archives],input=''.join(n+'\n' for n in overrides))
  expected=[(bytes.fromhex(l.split()[1]),int(l.split()[2])) for l in separate if l.startswith('M ')]
  for j,((raw_record,array),pair) in enumerate(zip(expected,maps,strict=True)):
   modified=bytearray(raw_record)
   for offset,(global_id,local_id) in zip((0x10,0xb4),pair):
    assert struct.unpack_from('<i',modified,offset)[0]==local_id
    struct.pack_into('<i',modified,offset,global_id)
   assert records[at+j]==(bytes(modified),array),(level['level'],binding['index'],j)
  at+=len(disk)
 assert at==header[1] and len(unique)==len(textures)
 total=0
 for key,slot in unique.items():
  pixels=(root/'artifacts/npc-texture-residency'/(hashlib.sha256(key.encode()).hexdigest()+'.rgba')).read_bytes()
  expected=next(t for t in reference['textures'] if t['name'].lower()==key)
  assert hashlib.sha256(pixels).hexdigest()==expected['sha256']
  h=2166136261
  for b in pixels:h=((h^b)*16777619)&0xffffffff
  assert textures[slot]==[slot,expected['width'],expected['height'],len(pixels),h]
  total+=len(pixels)
 assert total==level['decoded_bytes']
 results.append(dict(level=level['level'],appearances=header[0],materials=header[1],textures=header[2],resident_bytes=header[3],peak_bytes=header[4],pixel_bytes=total))
report=dict(result='PASS',scope='Actual opening archive materials: all shared handles, unchanged 200-byte material fields and scalar arrays versus separate appearance loads, independently decoded image hashes; exact/short budget, missing-image cleanup, and lifetime after input closure.',levels=results)
(root/'artifacts/npc-materials-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
