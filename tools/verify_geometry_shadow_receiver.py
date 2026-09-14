"""Retained L1S1 shadow receiver UV conversion vs original and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX
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
records=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(path),'L1S1.rfl','--shadow-receivers'])
x=machine(root/'build/xbox/main.exe');o=machine(exe);symbol=int(re.search(r'\s_rf_geometry_shadow_receiver\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
H=0x40000000;x.mem_map(H,4*1024*1024);x.mem_write(H,raw);F=H+((len(raw)+3)//4*4);x.mem_write(F,w(*offsets));G=B+0x1000;COUNT=B+0x2000
x.mem_write(G,w(H,len(raw),0,g['textures'],g['rooms'],g['vertices'],g['faces'],g['corners'],g['mappings'],g['vertices_offset'],g['mapping_offset'],0,0,0,F,0,0))
NODES=B+0x9000;RESULT=B+0xa000;at=0;polygons=points=0
while at<len(records):
 index,n=struct.unpack_from('<II',records,at);at+=8;got=records[at:at+n*8];at+=n*8;header,cs,mapping=faces[index];assert len(cs)==n
 mapping_raw=raw[g['mapping_offset']+mapping*96:g['mapping_offset']+(mapping+1)*96];ox,oy=mapping_raw[4:6];view=w(128,128,ox,oy)+bytes(40)
 o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+12,w(IMAGE,ox,oy));o.mem_write(IMAGE,w(0,128,128));o.mem_write(RESULT,bytes(260))
 for j,(v,uv) in enumerate(cs):o.mem_write(NODES+j*32,bytes(12)+uv+w(NODES+((j+1)%n)*32)+bytes(8))
 o.mem_write(STACK,bytes(128));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EAX,RESULT);o.reg_write(UC_X86_REG_ECX,NODES);o.reg_write(UC_X86_REG_EDX,NODES);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f49ae,0x4f4a12,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4f4a12 and bytes(o.mem_read(RESULT,n*8))==got and struct.unpack('<I',o.mem_read(RESULT+256,4))[0]==n,(index,'original')
 x.mem_write(VIEW,view);x.mem_write(OUT,bytes([165])*2048);x.mem_write(COUNT,w(0xa5a5a5a5));x.mem_write(STACK,w(STOP,G,index,VIEW,OUT,256,COUNT));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(symbol,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,n*8))==got and struct.unpack('<I',x.mem_read(COUNT,4))[0]==n,(index,'nxdk')
 polygons+=1;points+=n
for face_id,cap in [(index,0),(g['faces'],256)]:
 x.mem_write(OUT,bytes([165])*2048);x.mem_write(COUNT,w(0xa5a5a5a5));x.mem_write(STACK,w(STOP,G,face_id,VIEW,OUT,cap,COUNT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(symbol,STOP,count=100000);assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,2048))==bytes([165])*2048 and bytes(x.mem_read(COUNT,4))==w(0xa5a5a5a5)
report=dict(result='PASS',l1s1_original_pc_nxdk_receivers=polygons,vertices=points,pc_capacity_guards=polygons,nxdk_guards=2,original_sha256=sha,scope='Actual retained L1S1 lightmapped faces. Original4f49ae..4f4a12 receiver corner loop with supplied128x128 image and actual serialized mapping origins/UVs. No allocation or duplicate geometry table. Receiver selection, live image dimensions/ownership, runtime UV changes and native rendering excluded.')
(root/'artifacts/geometry-shadow-receiver.json').write_text(json.dumps(report,indent=2));print(report)
