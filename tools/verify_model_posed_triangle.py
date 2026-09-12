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
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_collision_model_posed_triangle\s+([0-9a-fA-F]+)',mapping)[1],16)
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
rng=random.Random(0x54e530);commands=[];answers=[];hits=later=0
for case in range(8192):
 axis=case%3;a=(axis+1)%3;c=(axis+2)%3;verts=[[0.,0.,0.] for _ in range(3)]
 for v,uv in zip(verts,((-2,-2),(2,-2),(0,2))):v[a],v[c]=uv
 if case%2:verts.reverse()
 start=[rng.uniform(-3,3) for _ in range(3)];delta=[rng.uniform(-4,4) for _ in range(3)]
 if case<4096:
  start[axis]=(-2.,-1.,0.,1.,2.)[case//6%5];delta[axis]=(-4.,-2.,0.,2.,4.)[case//30%5]
 else:verts=[[rng.uniform(-3,3) for _ in range(3)] for _ in range(3)]
 if case%37==0:verts[2]=verts[1][:]
 radius=(0,.02499999,.025,.02500001,.25,1)[case//150%6];limit=(0.,.25,.5,1.,2.)[case//900%5]
 # Caller endpoint can be clipped independently of the stored hit time.
 end=[start[i]+delta[i]*(limit if case%3 else 1) for i in range(3)]
 initial=f([limit,11,12,13,14,15,16])+w(0x12345678);command=f(start+delta+end+[v for row in verts for v in row]+[radius])+w(0x11223344)+initial;assert len(command)==112;commands.append(command)
 outputs=[]
 for m,native in ((u,False),(x,True)):
  m.mem_write(b,command+b'\xa5'*16);m.mem_write(b+0x1000,b'\xa5'*104);m.mem_write(b+0x1048,f([radius]));m.mem_write(b+0x1050,f(start+delta))
  rb=struct.unpack('<I',f([radius]))[0]
  args=(b+36,b,b+12,b+24,rb,0x11223344,b+80) if native else (0x11223344,b+36,b+48,b+60,b+0x1000,b+80,b+24)
  m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(entry if native else 0x54e530,stop,count=100000)
  assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
  assert bytes(m.mem_read(b,80))==command[:80] and bytes(m.mem_read(b+112,16))==b'\xa5'*16
  outputs.append(w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(b+80,32)))
 assert outputs[0]==outputs[1],('NXDK',case,command.hex(),[o.hex() for o in outputs])
 expected=outputs[0];answers.append(expected);accepted=struct.unpack('<I',expected[:4])[0];hits+=accepted
 if accepted:later+=struct.unpack_from('<f',expected,4)[0]>=limit
 else:assert expected[4:]==initial
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-posed-triangle'],input=b''.join(commands))
for case,expected in enumerate(answers):assert actual[case*36:case*36+36]==expected,('PC',case,actual[case*36:case*36+36].hex(),expected.hex())
assert len(actual)==len(answers)*36
report=dict(result='PASS',cases=len(commands),hits=hits,non_nearer_hits=later,original_sha256=sha,scope='Full54e530 with actual cross/normalize/plane/bounds/thin/sphere/edge callees; constructor flags initialized only. Exact PC/NXDK result and low return, preserved misses/inputs; axis/arbitrary/degenerate triangles,0.025 threshold, independently clipped endpoint and nearest-hit exception. No pose traversal or live/XEMU integration.')
(root/'artifacts/model-posed-triangle.json').write_text(json.dumps(report,indent=2));print(report)
