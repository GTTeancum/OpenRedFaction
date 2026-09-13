"""Retained L1S1 corner binding vs original smoothing and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;VIEW=B+0x6000;OWNER=B+0x7000;IMAGE=B+0x7100;DIRTY=B+0x7200;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u

from inspect_geometry import inspect
inventory=json.loads((root/'artifacts/inventory.json').read_text());level=next(v for v in json.loads((root/'artifacts/levels.json').read_text()) if v['file']=='L1S1.rfl');archive=next(v for v in inventory['files'] if v['path']==level['archive']);entry=next(v for v in archive['vpp']['entries'] if v['name']==level['file']);section=next(v for v in level['sections'] if v['type']=='0x100');path=root/'Installed_Game'/level['archive']
with path.open('rb') as stream:stream.seek(entry['offset']+section['offset']+8);raw=stream.read(section['size'])
g=inspect(raw);at=g['vertices_offset']+g['vertices']*12+4;faces=[];offsets=[];adj=[[] for _ in range(g['vertices'])]
for face in range(g['faces']):
 offsets.append(at);header=raw[at:at+56];mapping=struct.unpack_from('<I',header,20)[0];count=struct.unpack_from('<I',header,52)[0];at+=56;stride=12 if mapping==0xffffffff else 20;corners=[]
 for j in range(count):
  v=struct.unpack_from('<I',raw,at)[0];corners.append((v,raw[at+12:at+20] if stride==20 else bytes(8)));at+=stride
  if face not in adj[v]:adj[v].append(face)
 faces.append((header,corners,mapping))

records=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--lightmap-vertices','262144']);corner_values={}
for i in range(len(records)//44):
 face,corner,status=struct.unpack_from('<3I',records,i*44);assert status==0;corner_values[face,corner]=records[i*44+12:i*44+44]
grouped=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--lightmap-polygons','65536']);cursor=0;groups=[];total=0;peak=0
for mapping in range(g['mappings']):
 room=struct.unpack_from('<i',raw,g['mapping_offset']+mapping*96+92)[0];selected=[f for f in range(len(faces)) if (faces[f][2]&65535)==mapping and (room==-1 or struct.unpack_from('<I',faces[f][0],48)[0]==room)]
 identity,np,nv=struct.unpack_from('<3I',grouped,cursor);cursor+=12;assert identity==mapping and np==len(selected) and nv==sum(len(faces[f][1]) for f in selected)
 expected=[]
 for f in selected:
  n=struct.unpack_from('<I',grouped,cursor)[0];cursor+=4;assert n==len(faces[f][1]);values=b''.join(corner_values[f,j] for j in range(n));assert grouped[cursor:cursor+n*32]==values;cursor+=n*32;expected.append(values)
 groups.append((room,expected,nv));total+=nv;peak=max(peak,np*8+nv*32)
assert cursor==len(grouped)
x=machine(root/'build/xbox/main.exe');o=machine(exe);symbol=int(re.search(r'\s_rf_geometry_lightmap_polygons\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
H=0x40000000;x.mem_map(H,4*1024*1024);x.mem_write(H,raw);F=H+((len(raw)+3)//4*4);x.mem_write(F,w(*offsets));A=F+len(offsets)*4;starts=[0];links=[]
for values in adj:links+=values;starts.append(len(links))
x.mem_write(A,w(*starts));L=A+len(starts)*4;x.mem_write(L,w(*links));G=B+0x1000;OWNER_X=B+0x2000;WORK=B+0x3000
x.mem_write(G,w(H,len(raw),0,g['textures'],g['rooms'],g['vertices'],g['faces'],g['corners'],g['mappings'],g['vertices_offset'],g['mapping_offset'],0,0,0,F,0,0));x.mem_write(OWNER_X,w(A,L,g['vertices'],len(links),0))

IDS=H+3*1024*1024;VERTICES=IDS+65536;POLYGONS=B+0x6000;GROUP_WORK=B+0x5000;x.mem_write(IDS,w(*range(g['faces'])))
checks=sorted(set([0,g['mappings']-1]+list(range(0,g['mappings'],173))))
for mapping in checks:
 room,expected,nv=groups[mapping];np=len(expected)
 x.mem_write(GROUP_WORK,w(POLYGONS,VERTICES,WORK,np,nv,24));x.mem_write(OUT,bytes([165])*8)
 x.mem_write(STACK,w(STOP,G,OWNER_X,IDS,g['faces'],mapping,room&0xffffffff,GROUP_WORK,OUT,OUT+4));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(symbol,STOP,count=20000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,8))==w(np,nv),mapping
 for j,values in enumerate(expected):
  pointer,count=struct.unpack('<2I',x.mem_read(POLYGONS+j*8,8));assert count*32==len(values) and bytes(x.mem_read(pointer,count*32))==values
report=dict(result='PASS',pc_mapping_groups=g['mappings'],pc_vertices=total,nxdk_groups=len(checks),peak_polygon_vertex_scratch_bytes=peak,scope='L1S1 retained grouping compared to file-order mapping/room filtering and individually verified corner outputs. PC count queries and one-vertex-short guards checked; compiled NXDK sampled mapping groups match pointers/counts/contents. Loader survival and native rendering remain outside scope.')
(root/'artifacts/geometry-lightmap-polygons.json').write_text(json.dumps(report,indent=2));print(report)
