"""Original54e000 traversal/preparation versus PC/NXDK; only54daa0 supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_collision_model_part_trace\s+([0-9a-fA-F]+)',mapping)[1],16)
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
model=b+0x1000;part=b+0x1100;table=b+0x1300;query=b+0x2000;hit=b+0x3000
u.mem_write(model+0x4c,w(part));u.mem_write(part+0x8c,w(table))
rng=random.Random(0x54daa0);commands=[];answers=[];hits=0
for case in range(4096):
 flags=case%4;reset=(0,1,0x101,2)[case//4%4];fallback=(0,0x10)[case//16%2];radius=(0,.00009999,.0001,.25,.5)[case//32%5]
 origin=[rng.randrange(-8,9)/4 for _ in range(3)];offset=[rng.randrange(-8,9)/4 for _ in range(3)]
 matrix=([1,0,0,0,1,0,0,0,1],[0,1,0,-1,0,0,0,0,1],[1,0,0,0,0,1,0,-1,0])[case//160%3]
 start=[rng.randrange(-16,17)/4 for _ in range(3)];delta=[rng.randrange(-24,25)/4 for _ in range(3)]
 if case%3==0:start=[offset[0],offset[1],offset[2]+2];delta=[0,0,-4]
 q=f(origin+list(matrix)+start+delta+[radius])+w(flags)+b'\xa5'*24;initial=f([(0,.25,.75,1,2)[case%5],11,12,13,14,15,16])+w(0x12345678)
 metadata=f(offset+[-3,-3,-3,3,3,3]);counts=[case//7%3,case//11%3];wires=[]
 u.mem_write(table,w(2,b+0x4000,b+0x4100));u.mem_write(table+0x1c,f(offset));u.mem_write(table+0x2c,metadata[12:])
 for l in range(2):
  lod=b+0x4000+l*0x100;u.mem_write(lod,b'\x00'*0x44);u.mem_write(lod+8,w(b+0x5000+l*0x200));u.mem_write(lod+12,struct.pack('<H',counts[l]));u.mem_write(lod+0x40,w(fallback if l else 0))
 for n in range(4):
  vertex=[];planes=[];records=[];count=case//(n+1)%4
  for t in range(3):
   z=([-1,0,1],[1,0,-1])[case%2][t];normal=1 if (case+n+t)%5 else -1
   vertex.extend([-2,-2,z,2,-2,z,0,2,z]);planes.extend([0,0,normal,-z*normal]);records.append(struct.pack('<4H',t*3,t*3+1,t*3+2,0x20 if (case+t)%2 else 0))
  wire=f(vertex+planes)+b''.join(records)+w(count);assert len(wire)==184;wires.append(wire)
  batch=b+0x5000+(n//2)*0x200+(n%2)*0x38;vp=b+0x6000+n*0x200;pp=b+0x7000+n*0x100;rp=b+0x8000+n*0x100
  u.mem_write(batch,b'\x00'*0x38);u.mem_write(batch+4,w(vp));u.mem_write(batch+0x10,w(pp,rp));u.mem_write(batch+0x2a,struct.pack('<H',count));u.mem_write(vp,wire[:108]);u.mem_write(pp,wire[108:156]);u.mem_write(rp,wire[156:180])
 command=q+initial+w(reset)+metadata+w(fallback,*counts)+b''.join(wires);assert len(command)==924;commands.append(command)
 u.mem_write(query,q);u.mem_write(hit,initial);before=bytes(u.mem_read(b+0x4000,0x5000))
 u.mem_write(stack,w(stop,0,query,hit,reset));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,model)
 u.emu_start(0x54daa0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+20
 accepted=u.reg_read(UC_X86_REG_EAX)&255;expected=w(accepted)+bytes(u.mem_read(query,104))+bytes(u.mem_read(hit,32));answers.append(expected);hits+=accepted
 assert bytes(u.mem_read(query,80))==q[:80] and bytes(u.mem_read(b+0x4000,0x5000))==before
 x.mem_write(b,command+b'\xa5'*16)
 for n in range(4):
  p=b+188+n*184;x.mem_write(b+0xb000+n*20,w(p,p+108,p+156,b+0x8000+n*0x100)+struct.pack('<HH',struct.unpack_from('<I',wires[n],180)[0],0))
 for l in range(2):x.mem_write(b+0xb100+l*12,w(b+0xb000+l*40,fallback if l else 0)+struct.pack('<HH',counts[l],0))
 x.mem_write(b+0xb200,metadata+w(b+0xb10c,b+0xb100));x.mem_write(stack,w(stop,b+0xb200,b,b+104,reset));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b,136));assert actual==expected,('NXDK',case,expected.hex(),actual.hex())
 assert bytes(x.mem_read(b+136,788))==command[136:] and bytes(x.mem_read(b+924,16))==b'\xa5'*16
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-part-trace'],input=b''.join(commands))
for case,expected in enumerate(answers):assert actual[case*140:case*140+140]==expected,('PC',case)
assert len(actual)==len(answers)*140
report=dict(result='PASS',cases=len(commands),hits=hits,original_sha256=sha,scope='Complete54daa0 including actual54dcd0 thin/sphere triangle/math callees; static constructor flags preinitialized only. PC/NXDK exact104-byte query and32-byte hit plus return,0..2 batches and0..3 triangles,LOD fallback, rotations, offsets, reset/first-hit flags,radius threshold and unchanged geometry. Borrowed resolved views; no live owner binding/native XEMU.')
(root/'artifacts/model-part-trace.json').write_text(json.dumps(report,indent=2));print(report)
