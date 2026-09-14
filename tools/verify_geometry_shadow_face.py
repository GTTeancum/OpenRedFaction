"""Retained L1S1 shadow face snapshots vs original finalization and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ECX
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
records=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--shadow-faces']);assert len(records)==g['faces']*64
x=machine(root/'build/xbox/main.exe');o=machine(exe);symbol=int(re.search(r'\s_rf_geometry_shadow_face\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
H=0x40000000;x.mem_map(H,4*1024*1024);x.mem_write(H,raw);F=H+((len(raw)+3)//4*4);x.mem_write(F,w(*offsets));G=B+0x1000;WORK=B+0x3000
x.mem_write(G,w(H,len(raw),0,g['textures'],g['rooms'],g['vertices'],g['faces'],g['corners'],g['mappings'],g['vertices_offset'],g['mapping_offset'],0,0,0,F,0,0))
PLANE=B+0x8000;NODES=B+0x9000;VERTICES=B+0xa000;rejected=0;max_corners=0
for i in range(g['faces']):
 index,status=struct.unpack_from('<II',records,i*64);got=records[i*64+8:i*64+64];assert index==i and status==0
 header,cs,mapping=faces[i];max_corners=max(max_corners,len(cs));o.mem_write(OWNER,bytes(128));o.mem_write(OWNER+0x40,w(NODES));o.mem_write(PLANE,header[:16])
 for j,(v,uv) in enumerate(cs):
  point=raw[g['vertices_offset']+v*12:g['vertices_offset']+(v+1)*12];o.mem_write(VERTICES+j*12,point);o.mem_write(NODES+j*32,w(VERTICES+j*12)+bytes(16)+w(NODES+((j+1)%len(cs))*32,NODES+((j-1)%len(cs))*32))
 o.mem_write(STACK,w(STOP,PLANE,0));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,OWNER);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4dfe20,STOP,count=100000);assert o.reg_read(UC_X86_REG_EIP)==STOP;rejected+=not(o.reg_read(UC_X86_REG_EAX)&255)
 signed=lambda v:v-65536 if v>=32768 else v
 portal=struct.unpack_from('<I',header,36)[0]&65535;flags=struct.unpack_from('<I',header,40)[0];expected=bytes(o.mem_read(OWNER,40))+w(flags,signed(mapping&65535)&0xffffffff,signed(portal)&0xffffffff,0);assert got==expected,(i,'original')
 x.mem_write(OUT,bytes([165])*56);x.mem_write(STACK,w(STOP,G,i,WORK,256,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(symbol,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,56))==got,(i,'nxdk')
for cap,index in [(0,0),(256,g['faces'])]:
 x.mem_write(OUT,bytes([165])*56);x.mem_write(STACK,w(STOP,G,index,WORK,cap,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=100000);assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,56))==bytes([165])*56
report=dict(result='PASS',l1s1_original_pc_nxdk_faces=g['faces'],pc_capacity_guards=g['faces'],nxdk_guards=2,original_rejected_faces=rejected,largest_face_vertex_scratch_bytes=max_corners*12,original_sha256=sha,scope='Actual retained L1S1 geometry on PC and compiled NXDK. Plane/bounds match full unhooked4dfe20 supplied-file-plane finalization; flags/signed metadata match serialized records. Caller vertex scratch, no allocation. Snapshot does not choose loader survivors or apply subsequent runtime mutations; texture classification and native shadow rendering remain external.')
(root/'artifacts/geometry-shadow-face.json').write_text(json.dumps(report,indent=2));print(report)
