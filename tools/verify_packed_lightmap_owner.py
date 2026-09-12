"""Authored packed-map ownership, exact pixel conversion and face-color binding."""
import json,struct,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];exe=root/'build/pc/Release/rf_collision_probe.exe'
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());rows=[]
f32=lambda value:struct.unpack('<f',struct.pack('<f',value))[0]
for name in ['L1S1.rfl','L1S2.rfl','L1S3.rfl']:
 level=next(l for l in levels if l['file']==name);archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==name)
 section=next(s for s in level['sections'] if s['type']=='0x1200')
 with (root/'Installed_Game'/level['archive']).open('rb') as stream:
  stream.seek(entry['offset']+section['offset']+8);raw=stream.read(section['size'])
 for mode in [0,1]:
  data=subprocess.check_output([str(exe),'--level-packed-lightmaps',str(root/'Installed_Game'/level['archive']),name,str(mode)])
  count,allocated=struct.unpack_from('<2I',data);assert count==struct.unpack_from('<I',raw)[0];at=8;source_at=4;images=[];pixel_bytes=0
  for image in range(count):
   width,height,pitch,length=struct.unpack_from('<4I',data,at);at+=16
   sw,sh=struct.unpack_from('<2I',raw,source_at);source_at+=8;assert (width,height,pitch,length)==(sw,sh,sw*2,sw*sh*2)
   rgb=raw[source_at:source_at+width*height*3];source_at+=len(rgb)
   if mode:rgb=bytes(min(v*2+1,255) for v in rgb)
   expected=bytearray(length)
   for i in range(width*height):
    r,g,b=(max(v>>3,4) for v in rgb[i*3:i*3+3]);struct.pack_into('<H',expected,i*2,0x8000|(r<<10)|(g<<5)|b)
   pixels=data[at:at+length];at+=length;assert pixels==expected,(name,mode,image)
   images.append((width,height,pitch,pixels));pixel_bytes+=length
  assert source_at==len(raw) and allocated==12+count*20+pixel_bytes
  samples=struct.unpack_from('<I',data,at)[0];at+=4;white=0;range_errors=0
  for sample in range(samples):
   face,mapping,image,status=struct.unpack_from('<IIIi',data,at);point=struct.unpack_from('<3f',data,at+16);color=struct.unpack_from('<I',data,at+28)[0]
   ax0,ax1,s0,s1,o0,o1=struct.unpack_from('<II4f',data,at+32);at+=56
   if mapping==0xffffffff:assert status==0 and color==0xffffffff;white+=1;continue
   uv=[min(1,max(0,f32(f32(point[axis]*scale)+offset))) for axis,scale,offset in zip([ax0,ax1],[s0,s1],[o0,o1])]
   width,height,pitch,pixels=images[image];offset=int(width*uv[0])*2+int(height*uv[1])*pitch
   if offset+2>len(pixels):assert status==-4 and color==0x12345678;range_errors+=1;continue
   pixel=struct.unpack_from('<H',pixels,offset)[0];expected=0xff000000|(((pixel>>10)&31)<<3)|(((pixel>>5)&31)<<11)|((pixel&31)<<19)
   assert status==0 and color==expected,(name,mode,face,status,hex(color),hex(expected))
  assert at==len(data)
  rows.append(dict(level=name,double_rgb=mode,images=count,pixel_bytes=pixel_bytes,accounted_bytes=allocated,face_samples=samples,white=white,bounded_range_errors=range_errors))
  print(rows[-1],flush=True)
report=dict(result='PASS',pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),rows=rows,scope='PC real-archive packed owners on three opening levels, both explicit conversion modes; all pixels compared to file RGB, exact-budget and one-byte-short cleanup checks, repeat close, first64 face centroid color samples per run. Combined authored callback checked against independently computed projection/texel results. Primitive original/NXDK equivalence is tested separately. No active renderer-capability selection, GPU texture sharing, native allocation test, corpse surface dispatch or rendering.')
(root/'artifacts/lightmap-packed-owner.json').write_text(json.dumps(report,indent=2)+'\n')
