"""Header-only world/mover texture demand; no game budget changes or pixel allocation."""
import json,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];level=sys.argv[1] if len(sys.argv)>1 else 'L1S2.rfl'
inv=json.loads((root/'artifacts/inventory.json').read_text())['files']
archive=next(a for a in inv if any(e['name'].lower()==level.lower() for e in a.get('vpp',{}).get('entries',[])))
run=subprocess.check_output([str(root/'build/pc/Release/rf_material_probe.exe'),'--texture-names',str(root/'Installed_Game'/archive['path']),level,'0'],text=True)
rows=run.splitlines();groups=int(rows[0].split()[1]);names=[r.split(' ',3)[3] for r in rows[1:]]
unique=list(dict.fromkeys(n.lower() for n in names));textures=[]
for name in unique:
 found=None
 for path in ['maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp']:
  a=next(a for a in inv if a['path']==path)
  e=next((e for e in a['vpp']['entries'] if e['name'].lower()==name),None)
  if e:found=(path,e);break
 if not found:textures.append(dict(name=name,missing=True));continue
 path,e=found
 with (root/'Installed_Game'/path).open('rb') as f:f.seek(e['offset']);h=f.read(32)
 if h[:4]==b'.vbm':
  version,w,height,fmt,fps,frames,mips=struct.unpack_from('<7I',h,4)
  details=dict(kind='VBM',version=version,format=fmt,frames=frames,extra_mips=mips)
 else:
  w,height=struct.unpack_from('<HH',h,12);details=dict(kind='TGA',bits=h[16],type=h[2])
 textures.append(dict(name=name,archive=path,width=w,height=height,base_rgba_bytes=w*height*4,**details))
pixels=sum(t.get('base_rgba_bytes',0) for t in textures)
# NXDK 32-bit owner/slot/pointer layout, matching current C structures.
base=40+(groups+1)*4+len(names)*4;slots=len(unique)*28;scratch=len(names)*65
report=dict(level=level,geometry_count=groups,texture_references=len(names),unique_textures=len(unique),base_rgba_bytes=pixels,nxdk_mapping_bytes=base,nxdk_texture_slot_bytes=slots,nxdk_name_scratch_bytes=scratch,nxdk_material_peak_bytes=pixels+base+slots+scratch,current_xbox_gpu_copy_bytes=pixels,textures=textures,scope='Header demand including unsupported formats; not proof of decoding or total Xbox process memory.')
if '--verify-load' in sys.argv:
 args=[str(root/'build/pc/Release/rf_material_probe.exe'),'--residency',str(root/'Installed_Game'/archive['path']),level,str(report['nxdk_material_peak_bytes'])]
 args += [str(root/'Installed_Game'/p) for p in ['maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp']]
 loaded=subprocess.check_output(args,text=True);summary=list(map(int,loaded.splitlines()[0].split()[1:]))
 assert summary==[groups,len(unique),sum(not t.get('missing',False) for t in textures),sum(t.get('missing',False) for t in textures),pixels+base+slots,pixels+base+slots+scratch],summary
 report['pc_load_exact_budget_verified']=True
out=root/'artifacts/texture-residency';out.mkdir(exist_ok=True);(out/(level+'.json')).write_text(json.dumps(report,indent=2))
print(json.dumps({k:v for k,v in report.items() if k!='textures'},indent=2))
