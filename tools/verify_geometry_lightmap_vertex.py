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
records=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--lightmap-vertices','262144']);assert len(records)%44==0
x=machine(root/'build/xbox/main.exe');o=machine(exe);symbol=int(re.search(r'\s_rf_geometry_lightmap_vertex\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
H=0x40000000;x.mem_map(H,4*1024*1024);x.mem_write(H,raw);F=H+((len(raw)+3)//4*4);x.mem_write(F,w(*offsets));A=F+len(offsets)*4;starts=[0];links=[]
for values in adj:links+=values;starts.append(len(links))
x.mem_write(A,w(*starts));L=A+len(starts)*4;x.mem_write(L,w(*links));G=B+0x1000;OWNER_X=B+0x2000;WORK=B+0x3000
x.mem_write(G,w(H,len(raw),0,g['textures'],g['rooms'],g['vertices'],g['faces'],g['corners'],g['mappings'],g['vertices_offset'],g['mapping_offset'],0,0,0,F,0,0));x.mem_write(OWNER_X,w(A,L,g['vertices'],len(links),0))
VERTEX=B+0x8000;NODE=B+0x8100;TABLE=B+0x8200;FACES=B+0x9000
for i in range(len(records)//44):
 face,corner,status=struct.unpack_from('<3I',records,i*44);got=records[i*44+12:i*44+44];assert status==0,(face,corner,status)
 header,cs,mapping=faces[face];v,uv=cs[corner];assert got[:20]==uv+raw[g['vertices_offset']+v*12:g['vertices_offset']+(v+1)*12]
 o.mem_write(OWNER,bytes(80));o.mem_write(OWNER,header[:12]);o.mem_write(OWNER+60,w(len(cs)));o.mem_write(NODE,w(VERTEX));o.mem_write(VERTEX+32,w(len(adj[v]),len(adj[v]),TABLE))
 for j,index in enumerate(adj[v]):
  p=OWNER if index==face else FACES+j*80
  if p!=OWNER:o.mem_write(p,bytes(80));o.mem_write(p,faces[index][0][:12]);o.mem_write(p+60,w(len(faces[index][1])))
  o.mem_write(TABLE+j*4,w(p))
 o.mem_write(STACK,bytes(256));o.mem_write(STACK+28,w(OUT));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,NODE);o.reg_write(UC_X86_REG_EBX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f4192,0x4f4222,count=100000)
 assert got[20:]==bytes(o.mem_read(STACK+32,12)),(face,corner,'original')
 x.mem_write(STACK,w(STOP,G,OWNER_X,face,corner,WORK,24,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(symbol,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,32))==got,(face,corner,'nxdk')
for cap,index in [(0,face),(24,g['faces'])]:
 x.mem_write(OUT,bytes([165])*32);x.mem_write(STACK,w(STOP,G,OWNER_X,index,corner,WORK,cap,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=100000);assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,32))==bytes([165])*32
report=dict(result='PASS',l1s1_original_pc_nxdk_corners=len(records)//44,nxdk_guards=2,scratch_bytes=24*20,original_sha256=sha,scope='Actual L1S1 retained PC geometry and adjacency binding; position/UV from serialized records, normal from original4f4192..4f4222 unhooked helper traversal; compiled NXDK binding reads same geometry/adjacency. All serialized faces supplied as surviving; loader rejection/plane repair and native residency/rendering excluded.')
(root/'artifacts/geometry-lightmap-vertex.json').write_text(json.dumps(report,indent=2));print(report)
