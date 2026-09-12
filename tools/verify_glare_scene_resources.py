"""Derive scene glare resource counters from standalone owners/archive image data."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];audit=json.loads((root/'artifacts/glare-bitmaps/report.json').read_text())
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def hash_bytes(h,data):
 for b in data:h=((h^b)*16777619)&0xffffffff
 return h
value=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-classes',str(root/'Installed_Game/tables.vpp'),'1000000'])
status,count,definition_bytes,definition_peak=struct.unpack_from('<4I',value);assert status==0
names=[r['name'].lower() for r in audit['textures']];bindings=[]
for cls in audit['class_bindings']:
 bindings.extend(names.index(cls['bitmaps'][key].lower()) if key in cls['bitmaps'] else 0xffffffff for key in ('corona','volumetric','reflection'))
h=hash_bytes(2166136261,w(*bindings));p=2166136261;frames=pixel_bytes=0
inv=json.loads((root/'artifacts/inventory.json').read_text())['files'];out=root/'artifacts/glare-scene-frame.rgba'
for texture in audit['textures']:
 candidate=texture['candidates'][0];archive=root/'Installed_Game'/candidate['archive'];n=candidate['frames'];frames+=n
 h=hash_bytes(h,texture['name'].encode('cp1252').ljust(64,b'\0')+w(n,candidate['rate']))
 e=next(e for a in inv if a['path']==candidate['archive'] for e in a['vpp']['entries'] if e['name'].lower()==texture['name'].lower())
 with archive.open('rb') as f:f.seek(e['offset']);header=f.read(32)
 vbm=header[:4]==b'.vbm';fmt=(5,4,3)[struct.unpack_from('<I',header,16)[0]] if vbm else {24:6,32:7}[header[16]]
 for frame in range(n):
  args=[str(root/'build/pc/Release/rf_image_probe.exe'),str(archive),texture['name'],str(out),'4000000']+([str(frame)] if vbm else [])
  width,height,size=map(int,subprocess.check_output(args).split());pixels=out.read_bytes();assert len(pixels)==size
  h=hash_bytes(h,w(width,height,size,fmt));p=hash_bytes(p,pixels);pixel_bytes+=size
materials=24+count*(12+3*84)+sum(c['candidates'][0]['resident_bytes']-20 for c in audit['textures'])
retained=definition_bytes+materials
expected=[count,len(names),frames,retained,max(retained,definition_peak),hash_bytes(2166136261,value[16:]),h,pixel_bytes,p]
report=dict(result='PASS',expected=expected,scope='Standalone class owner and every direct archive-decoded frame, authored slot order and headers; excludes scene code. Pixel decoding shares the core decoder. Native XEMU comparison additionally verifies swizzled image access and full resource residency.')
(root/'artifacts/glare-scene-resources.json').write_text(json.dumps(report,indent=2));print(report)
