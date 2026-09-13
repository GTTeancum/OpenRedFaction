"""Unhooked node eligibility/preparation and alternate edge geometry versus PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;NODES=[B+i*0x100 for i in range(8)];Q=B+0x1000;LIST=B+0x2000;REFS=B+0x3000;POINT=B+0x4000;OUT=B+0x5000;STACK=B+0xe000;STOP=B+0xf000
r=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');maps=(root/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',maps)[1],16)
prepare=symbol('rf_entity_navigation_search_prepare');edge=symbol('rf_entity_navigation_edge_allowed')
def call(m,entry,args,pop=0):
 m.mem_write(STACK,w(STOP,*args));m.reg_write(UC_X86_REG_ESP,STACK);m.reg_write(UC_X86_REG_ECX,LIST);m.reg_write(UC_X86_REG_FPCW,0x27f);m.emu_start(entry,STOP,count=100000);assert m.reg_read(UC_X86_REG_EIP)==STOP and m.reg_read(UC_X86_REG_ESP)==STACK+4+pop;return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4ce4b0);commands=[];expected=[];eligible=0
for case in range(2048):
 count=case%9;mode=rng.choice((0,1,2,256,257));radius,height=[rng.uniform(.1,6) for _ in range(2)];nodes=[]
 for i in range(8):
  n=bytearray(rng.randbytes(68));n[:12]=f(*[rng.uniform(-100,100) for j in range(3)]);n[28:36]=f(rng.uniform(.1,8),rng.uniform(.1,8));n[64:68]=w(rng.choice((0,0,1,256)));nodes.append(bytes(n))
 for m in (u,x):
  for a,n in zip(NODES,nodes):m.mem_write(a,n)
 u.mem_write(Q,bytes(64));u.mem_write(Q+24,f(radius,height));u.mem_write(Q+32,bytes([mode&255]));u.mem_write(LIST,w(count,8,REFS));u.mem_write(REFS,w(*NODES));call(u,0x4ce4b0,[Q],4)
 after=b''.join(bytes(u.mem_read(a,68)) for a in NODES);eligible+=sum(u.mem_read(a+53,1)==b'\0' for a in NODES[:count]);output=w(0)+after
 x.mem_write(REFS,b''.join(w(a,a,0,0) for a in NODES));status=call(x,prepare,[REFS,count,*struct.unpack('<2I',f(radius,height)),mode]);assert w(status)+b''.join(bytes(x.mem_read(a,68)) for a in NODES)==output,('prepare NXDK',case)
 commands.append(w(count,mode)+f(radius,height)+b''.join(nodes));expected.append(output)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-node-prepare'],input=b''.join(commands));assert actual==b''.join(expected),'prepare PC'
commands=[];expected=[];accepted=0
for case in range(2048):
 coords=[rng.uniform(-20,20) for _ in range(9)];threshold=rng.choice((0,0x80000000,0x7fc12345,0x7f800000,0xff800000,0x3f800000,0x42c80000))
 if case%8==0:coords=[0,1,0,0,0,0,10,0,0];threshold=0x42c80000 # Projection at start always accepted.
 if case%8==1:coords=[5,1,0,0,0,0,10,0,0];threshold=0x3f800000 # Strict equality passes.
 if case%8==2:coords=[5,1,0,0,0,0,10,0,0];threshold=0x40000000 # Interior point below threshold rejects.
 if case%8==3:coords[6:9]=coords[3:6] # Coincident segment.
 command=f(*coords)+w(threshold)
 for m in (u,x):m.mem_write(POINT,command);m.mem_write(OUT,w(99))
 result=call(u,0x4ce6c0,[POINT,POINT+12,POINT+24,threshold])&255;assert result in (0,1);accepted+=result
 status=call(x,edge,[POINT,POINT+12,POINT+24,threshold,OUT]);output=w(0,result);assert w(status,r(x,OUT))==output,('edge NXDK',case)
 assert bytes(u.mem_read(POINT,40))==command and bytes(x.mem_read(POINT,40))==command
 commands.append(command);expected.append(output)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-edge'],input=b''.join(commands));assert actual==b''.join(expected),'edge PC'
# Bypass accepts without reading geometry; reached invalid geometry preserves output.
for threshold in (0,0x80000000,0x7fc12345):
 x.mem_write(OUT,w(99));assert call(x,edge,[0,0,0,threshold,OUT])==0 and r(x,OUT)==1
x.mem_write(POINT,w(0x7fc00000));x.mem_write(OUT,w(99));assert call(x,edge,[POINT,POINT+12,POINT+24,0x3f800000,OUT])!=0 and r(x,OUT)==99
report=dict(result='PASS',preparation_cases=2048,eligible_nodes=eligible,edge_cases=2048,accepted_edges=accepted,compiled_guards=4,scope='Full unhooked4ce4b0/4ce530 and4ce6c0 with actual projection/equality/distance. PC/NXDK exact node bytes and edge result; finite randomized geometry, mode bytes, zero/NaN threshold bypass, start projection, equality and coincident segment. No scene visibility or combined search wrapper.')
(root/'artifacts/ai-search-geometry.json').write_text(json.dumps(report,indent=2));print(report)
