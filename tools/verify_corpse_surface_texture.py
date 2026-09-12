"""Authored blood-pool texture owner: archive selection, pixels and budget."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];inventory=json.loads((root/'artifacts/inventory.json').read_text())
a=next(a for a in inventory['files'] if a['path']=='maps_en.vpp');e=next(e for e in a['vpp']['entries'] if e['name']=='somenewblood_A.tga')
with (root/'Installed_Game/maps_en.vpp').open('rb') as stream:stream.seek(e['offset']);raw=stream.read(e['size'])
assert raw[1:3]==bytes([0,2]) and raw[16]==32
width,height=struct.unpack_from('<2H',raw,12);pixels=bytearray(width*height*4);at=18+raw[0]
for y in range(height):
 for x in range(width):
  b,g,r,alpha=raw[at:at+4];at+=4
  tx=width-1-x if raw[17]&16 else x;ty=y if raw[17]&32 else height-1-y
  pixels[(ty*width+tx)*4:(ty*width+tx+1)*4]=bytes([r,g,b,alpha])
hash_value=2166136261
for value in pixels:hash_value=((hash_value^value)*16777619)&0xffffffff
probe=root/'build/pc/Release/rf_material_probe.exe'
def run(budget,archives):return list(map(int,subprocess.check_output([str(probe),'--corpse-texture',str(budget),*[str(root/'Installed_Game'/a) for a in archives]]).split()))
archives=['tables.vpp','maps_en.vpp'];info=run(1024*1024,archives)
assert info[0]==0,info
_,owner,frames,rate,w,h,resident,index,checksum,source_format=info
assert (frames,rate,w,h,resident,index,checksum,source_format)==(1,0,width,height,owner+len(pixels),1,hash_value,7),info
assert run(resident,archives)==info
assert run(resident-1,archives)==[-4,owner]
assert run(1024*1024,['tables.vpp'])==[-3,owner]
first=run(resident,['maps_en.vpp','tables.vpp']);assert first[:7]==info[:7] and first[7]==0 and first[8:]==info[8:]
report=dict(result='PASS',cases=5,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),width=width,height=height,pixel_bytes=len(pixels),resident_bytes=resident,pixel_fnv1a=checksum,source_format=source_format,alpha_min=min(pixels[3::4]),alpha_max=max(pixels[3::4]),scope='PC dedicated surface texture loader using existing bitmap owner. Independently decoded uncompressed32-bit TGA orientation/BGRA converted to RGBA checksum; exact and one-byte-short budget, missing asset, archive index/order, archive closure before inspection and repeated cleanup. NXDK compile only; no native GPU upload or live scene residency claim.')
(root/'artifacts/corpse-surface-texture.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
