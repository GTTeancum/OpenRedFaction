"""Measure unique authored NPC textures using real material records and decodes."""
import hashlib,json,subprocess
from pathlib import Path
from inspect_models import inspect
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game';output_dir=root/'artifacts/npc-texture-residency';output_dir.mkdir(exist_ok=True)
inventory=json.loads((root/'artifacts/inventory.json').read_text())['files']
entries={a['path']:{e['name'].lower():e for e in a.get('vpp',{}).get('entries',[])} for a in inventory}
archives=('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')
def read(archive,name):
 e=entries[archive][name.lower()]
 with (game/archive).open('rb') as f:f.seek(e['offset']);return f.read(e['size'])
def cstring(raw):return raw.split(b'\0',1)[0].decode('ascii')
decoded={};results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 out=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--skeletons',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level],text=True)
 appearances={}
 for line in out.splitlines():
  f=line.split('\t')
  if f[0]=='APPEARANCE':appearances[int(f[3])]=(f[4],f[5:])
 names={};appearance_rows=[];separate_bytes=0
 for index,(model,overrides) in sorted(appearances.items()):
  raw=read('meshes.vpp',model);materials=[]
  for section in inspect(raw)['sections']:
   if section['type']=='0x5355424d':
    materials.extend(raw[section['material_offset']+i*84:section['material_offset']+(i+1)*84] for i in range(section['materials']))
  assert not overrides or len(overrides)==len(materials),(level,model,len(overrides),len(materials))
  selected=[]
  for i,record in enumerate(materials):
   pair=[overrides[i] if overrides else cstring(record[:32]),cstring(record[48:80])]
   assert pair[0]
   for name in pair:
    if name:
     key=name.lower();names.setdefault(key,name);selected.append(key)
     if key not in decoded:
      archive=next((a for a in archives if key in entries[a]),None);assert archive,(name,'missing')
      target=output_dir/(hashlib.sha256(key.encode()).hexdigest()+'.rgba')
      run=subprocess.run([str(root/'build/pc/Release/rf_image_probe.exe'),str(game/archive),name,str(target),'16777216'],capture_output=True,text=True)
      assert run.returncode==0,(name,run.stdout,run.stderr)
      width,height,size=map(int,run.stdout.split());pixels=target.read_bytes();assert len(pixels)==size==width*height*4
      decoded[key]=dict(name=name,archive=archive,width=width,height=height,bytes=size,sha256=hashlib.sha256(pixels).hexdigest())
  material_run=subprocess.run([str(root/'build/pc/Release/rf_material_probe.exe'),'--model-skin',str(game/'meshes.vpp'),model,'16777216',*[str(game/a) for a in archives]],input=''.join(name+'\n' for name in overrides),capture_output=True,text=True)
  assert material_run.returncode==0,(level,index,model,'material construction',material_run.stdout,material_run.stderr)
  material_count,texture_count,resident,peak=map(int,material_run.stdout.splitlines()[0].split())
  assert material_count==len(materials) and texture_count==len(set(selected))
  cost=sum(decoded[k]['bytes'] for k in set(selected));separate_bytes+=cost
  appearance_rows.append(dict(index=index,model=model,materials=len(materials),textures=selected,decoded_bytes=cost,bundle_resident_bytes=resident,bundle_peak_bytes=peak))
 total=sum(decoded[k]['bytes'] for k in names)
 results.append(dict(level=level,appearances=len(appearances),unique_textures=len(names),decoded_bytes=total,separate_appearance_bytes=separate_bytes,saved_by_global_dedup=separate_bytes-total,bindings=appearance_rows))
report=dict(result='PASS',scope='Authored primary replacements and unchanged secondary names from actual84-byte material records; first-match campaign map archive order; all unique textures decoded by current static loader and all appearance material instances constructed by current model loader. RGBA base images only: excludes material/slot metadata, allocator overhead, GPU duplicates and additional mip/frame residency.',levels=results,textures=list(decoded.values()))
(root/'artifacts/npc-texture-residency.json').write_text(json.dumps(report,indent=2));print(json.dumps(dict(result='PASS',levels=[{k:v for k,v in r.items() if k!='bindings'} for r in results],decoded_unique_across_levels=len(decoded)),indent=2))
