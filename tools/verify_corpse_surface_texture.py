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
raster_probe=root/'build/pc/Release/rf_particle_pixel_probe.exe'
raster_rows=[list(map(int,line.split())) for line in subprocess.check_output([str(raster_probe),'--corpse-texture',str(root/'Installed_Game/maps_en.vpp')],text=True).splitlines()]
assert len(raster_rows)==5 and all(len(row)==3 and row[0]==i for i,row in enumerate(raster_rows))
assert raster_rows[0][1]==raster_rows[4][1]==0 and 0<raster_rows[1][1]<=raster_rows[2][1]
assert raster_rows[2][1:]==raster_rows[3][1:] and raster_rows[0][2]==raster_rows[4][2]
report['raster_rows']=raster_rows
report['raster_scope']='PC rasterizer through composed growth/projection/encoding and synchronous sink, using authored RGBA texture. Zero/mid/mature/repeated mature and fully occluded draws; all depth values preserved. Behind-camera polygons skip sink while retaining growth; sink errors propagate with extent retained. Behavioral integration, not an original GPU image comparison. Xbox callback compatibility/build only, no XEMU raster test.'
(root/'artifacts/corpse-surface-texture.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
