"""Full original506430 versus PC/NXDK, no substituted functions."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_model_segment_triangle\s+([0-9a-fA-F]+)',mapping)[1],16)
rng=random.Random(0x5065b0);commands=[];answers=[];hits=changed_miss=preserved=0
for case in range(8192):
 axis=case%3;a=(axis+1)%3;c=(axis+2)%3;verts=[[0.,0.,0.] for _ in range(3)]
 for v,uv in zip(verts,((-2,-2),(2,-2),(0,2))):v[a],v[c]=uv
 plane=[0.,0.,0.,0.];plane[axis]=(-1.,1.)[case%2]
 start=[rng.uniform(-3,3) for _ in range(3)];delta=[rng.uniform(-4,4) for _ in range(3)]
 if case<4096:start[axis]=(-2.,-1.,0.,1.,2.)[case//6%5];delta[axis]=(-4.,-2.,0.,2.,4.)[case//30%5]
 else:plane=[rng.uniform(-1,1) for _ in range(4)]
 seed=f([11,12,13,14]);command=f(start+delta+[n for v in verts for n in v]+plane)+seed;commands.append(command);outputs=[]
 for m,native in ((u,False),(x,True)):
  m.mem_write(b,command+b'\xa5'*16);m.mem_write(b+0x1000,w(b+24,b+36,b+48))
  args=(b,b+12,b+24,b+60,b+76) if native else (b,b+0x1000,b+60,b+76,b+88)
  m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(entry if native else 0x5065b0,stop,count=10000)
  assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
  assert bytes(m.mem_read(b,76))==command[:76] and bytes(m.mem_read(b+92,16))==b'\xa5'*16
  outputs.append(w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(b+76,16)))
 assert outputs[0]==outputs[1],('NXDK',case,command.hex(),[o.hex() for o in outputs])
 answers.append(outputs[0]);accepted=struct.unpack('<I',outputs[0][:4])[0];hits+=accepted
 if not accepted:changed_miss+=outputs[0][4:]!=seed;preserved+=outputs[0][4:]==seed
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-segment-triangle'],input=b''.join(commands));assert actual==b''.join(answers),'PC mismatch'
report=dict(result='PASS',cases=len(commands),hits=hits,misses_writing_intersection=changed_miss,misses_preserving=preserved,original_sha256=sha,scope='Full5065b0 with real506550/506dd0 and vector callees; no hooks. PC/NXDK exact point/fraction and low return, input/guard preservation, finite one-sided/coplanar/arbitrary planes. Not type2 triangle or live skeletal integration.')
(root/'artifacts/model-segment-triangle.json').write_text(json.dumps(report,indent=2));print(report)
