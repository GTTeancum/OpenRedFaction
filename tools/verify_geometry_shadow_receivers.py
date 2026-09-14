"""Retained receiver groups vs serialized selection and verified UV conversion."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x7100;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u

from inspect_geometry import inspect
inventory=json.loads((root/'artifacts/inventory.json').read_text());level=next(v for v in json.loads((root/'artifacts/levels.json').read_text()) if v['file']=='L1S1.rfl');archive=next(v for v in inventory['files'] if v['path']==level['archive']);entry=next(v for v in archive['vpp']['entries'] if v['name']==level['file']);section=next(v for v in level['sections'] if v['type']=='0x100');path=root/'Installed_Game'/level['archive']
with path.open('rb') as stream:stream.seek(entry['offset']+section['offset']+8);raw=stream.read(section['size'])
g=inspect(raw);at=g['vertices_offset']+g['vertices']*12+4;faces=[];offsets=[]
for face in range(g['faces']):
 offsets.append(at);header=raw[at:at+56];mapping=struct.unpack_from('<I',header,20)[0];count=struct.unpack_from('<I',header,52)[0];at+=56;stride=12 if mapping==0xffffffff else 20;corners=[]
 for j in range(count):
  v=struct.unpack_from('<I',raw,at)[0];corners.append((v,raw[at+12:at+20] if stride==20 else bytes(8)));at+=stride
 faces.append((header,corners,mapping))

light_section=next(v for v in level['sections'] if v['type']=='0x1200')
with path.open('rb') as stream:
 stream.seek(entry['offset']+light_section['offset']+8);light_raw=stream.read(light_section['size'])
image_count=struct.unpack_from('<I',light_raw)[0];images=[];at=4
for i in range(image_count):
 iw,ih=struct.unpack_from('<2I',light_raw,at);images.append((iw,ih));at+=8+iw*ih*3
assert at==len(light_raw)
receiver_values={}
for face,(header,corners,mapping) in enumerate(faces):
 if mapping==0xffffffff:continue
 record=raw[g['mapping_offset']+mapping*96:g['mapping_offset']+(mapping+1)*96]
 image=struct.unpack_from('<I',record)[0];iw,ih=images[image if image<image_count else 0]
 receiver_values[face]=b''.join(f(iw*struct.unpack('<2f',uv)[0]-record[4],ih*struct.unpack('<2f',uv)[1]-record[5]) for _,uv in corners)

grouped=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--shadow-receiver-groups','65536']);cursor=0;groups=[];total=0;peak=0
for mapping in range(g['mappings']):
 room=struct.unpack_from('<i',raw,g['mapping_offset']+mapping*96+92)[0];selected=[f for f in range(len(faces)) if (faces[f][2]&65535)==mapping and (room==-1 or struct.unpack_from('<I',faces[f][0],48)[0]==room)]
 identity,np,nv=struct.unpack_from('<3I',grouped,cursor);cursor+=12;assert identity==mapping and np==len(selected) and nv==sum(len(faces[f][1]) for f in selected)
 expected=[]
 for f in selected:
  n=struct.unpack_from('<I',grouped,cursor)[0];cursor+=4;assert n==len(faces[f][1]);values=receiver_values[f];assert grouped[cursor:cursor+n*8]==values;cursor+=n*8;expected.append(values)
 groups.append((room,expected,nv));total+=nv;peak=max(peak,np*8+nv*8)
assert cursor==len(grouped)
x=machine(root/'build/xbox/main.exe');symbol=int(re.search(r'\s_rf_geometry_shadow_receivers\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
H=0x40000000;x.mem_map(H,4*1024*1024);x.mem_write(H,raw);F=H+((len(raw)+3)//4*4);x.mem_write(F,w(*offsets));G=B+0x1000
x.mem_write(G,w(H,len(raw),0,g['textures'],g['rooms'],g['vertices'],g['faces'],g['corners'],g['mappings'],g['vertices_offset'],g['mapping_offset'],0,0,0,F,0,0))

IDS=H+3*1024*1024;VERTICES=IDS+65536;POLYGONS=B+0x6000;GROUP_WORK=B+0x5000;x.mem_write(IDS,w(*range(g['faces'])))
binding=int(re.search(r'\s_rf_geometry_lightmap_sample_binding\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
MAPS=B+0x8000;IMAGES=B+0x9000;MAPPING=B+0x7200
assert len(images)*20<0x4000
x.mem_write(IMAGES,b''.join(w(iw,ih,iw*ih*2,5,0) for iw,ih in images));x.mem_write(MAPS,w(IMAGES,len(images),0))
views=[]
for mapping in range(g['mappings']):
 record=raw[g['mapping_offset']+mapping*96:g['mapping_offset']+(mapping+1)*96]
 image=struct.unpack_from('<I',record)[0];image=image if image<len(images) else 0;iw,ih=images[image]
 expected=w(iw,ih,record[4],record[5])+record[84:92]+record[76:84]+record[40:56]+record[64:72]
 x.mem_write(STACK,w(STOP,G,MAPS,mapping,MAPPING,VIEW));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(binding,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(VIEW,56))==expected,mapping
 assert struct.unpack('<I',x.mem_read(MAPPING,4))[0]==image
 views.append(expected)
checks=sorted(set([0,g['mappings']-1]+list(range(0,g['mappings'],173))))
for mapping in checks:
 room,expected,nv=groups[mapping];np=len(expected);x.mem_write(VIEW,views[mapping])
 x.mem_write(GROUP_WORK,w(POLYGONS,VERTICES,np,nv));x.mem_write(OUT,bytes([165])*8)
 x.mem_write(STACK,w(STOP,G,IDS,g['faces'],mapping,room&0xffffffff,VIEW,GROUP_WORK,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(symbol,STOP,count=20000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,8))==w(np,nv),mapping
 for j,values in enumerate(expected):
  pointer,count=struct.unpack('<2I',x.mem_read(POLYGONS+j*8,8));assert count*8==len(values) and bytes(x.mem_read(pointer,count*8))==values
# Reject undersized scratch and invalid surviving ID order without publishing counts.
nonempty=next(i for i,v in enumerate(groups) if v[2]);room,expected,nv=groups[nonempty];np=len(expected)
for short_polygons in (False,True):
 x.mem_write(GROUP_WORK,w(POLYGONS,VERTICES,np-int(short_polygons),nv-int(not short_polygons)))
 x.mem_write(OUT,bytes([165])*8)
 x.mem_write(STACK,w(STOP,G,IDS,g['faces'],nonempty,room&0xffffffff,VIEW,GROUP_WORK,OUT,OUT+4))
 x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=20000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,8))==bytes([165])*8
x.mem_write(IDS,w(0,0));x.mem_write(OUT,bytes([165])*8)
x.mem_write(STACK,w(STOP,G,IDS,2,nonempty,room&0xffffffff,0,0,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=20000000)
assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,8))==bytes([165])*8
x.mem_write(STACK,w(STOP,G,0,0,nonempty,room&0xffffffff,0,0,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=20000000)
assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,8))==bytes(8)
# Exercise missing images, empty dimensions and rectangle bounds before publishing.
record0=raw[g['mapping_offset']:g['mapping_offset']+96]
x.mem_write(H+g['mapping_offset'],w(0xffffffff))
for size,count,success in ((128,len(images),True),(0,len(images),False),(1,len(images),False),(128,0,False)):
 x.mem_write(IMAGES,w(size,size));x.mem_write(MAPS,w(IMAGES,count,0))
 x.mem_write(VIEW,bytes([165])*56);x.mem_write(MAPPING,bytes([165])*108)
 x.mem_write(STACK,w(STOP,G,MAPS,0,MAPPING,VIEW));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(binding,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP
 if success:
  assert x.reg_read(UC_X86_REG_EAX)==0 and struct.unpack('<I',x.mem_read(MAPPING,4))[0]==0 and bytes(x.mem_read(VIEW,56))==views[0]
 else:
  assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(VIEW,56))==bytes([165])*56 and bytes(x.mem_read(MAPPING,108))==bytes([165])*108
x.mem_write(H+g['mapping_offset'],record0)
report=dict(result='PASS',pc_mapping_groups=g['mappings'],pc_vertices=total,nxdk_bindings=len(views),binding_guards=3,image_zero_fallback=1,image_dimensions=sorted(set(images)),nxdk_groups=len(checks),nxdk_guards=3,nxdk_empty_queries=1,peak_polygon_vertex_scratch_bytes=peak,scope='L1S1 retained grouping compared to file-order mapping/room filtering and independent receiver UV reconstruction with serialized image dimensions and origins. PC count queries and one-vertex-short guards checked; compiled NXDK sampled mapping groups match pointers/counts/contents. Loader survival and native rendering remain outside scope.')
(root/'artifacts/geometry-shadow-receivers.json').write_text(json.dumps(report,indent=2));print(report)
