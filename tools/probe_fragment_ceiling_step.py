"""Execute original kind3 contact steps; no original-game launch or screenshots."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
SHA='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
im=pefile.PE(str(exe)).get_memory_mapped_image();B=0x30000000;S=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
F=lambda v:struct.unpack('<f',f(v))[0]
I=(1,0,0,0,1,0,0,0,1);rows=[]
for normal in ((0,-1,0),(0,-.986393988,-.164398983),(0,1,0)):
 for fraction in (0,.5,1):
  for elasticity in (0,.6):
   for speed in (1,8):
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
    trace=[]
    def hook(cpu,address,size,data):
     if address in (0x4a01b0,0x48a400,0x412b40,0x49d330,0x417e00,0x49d280):trace.append(hex(address))
     assert address not in (0x49d7e0,0x4892c0,0x48ab40),('unexpected actor/damage path',hex(address))
    u.hook_add(UC_HOOK_CODE,hook)
    velocity=(1,-normal[1]*speed,-normal[2]*speed);end=tuple(F((1 if k==1 else 0)+velocity[k]*.1) for k in range(3))
    for off,val in ((0x24,3),(0x2c,0x10000),(0x7c,0),(0x1a8,0x8000003f),(0x1e4,0xffffffff)):
     u.mem_write(B+off,w(val))
    for off,val in ((0x34,123),(0x88,elasticity),(0x90,.5),(0x98,1.5),(0x180,.5),(0x1b0,.1),(0x1cc,fraction)):
     u.mem_write(B+off,f(val))
    for off,val in ((0x9c,I),(0xc0,I),(0xfc,I),(0x120,I),(0xe4,(0,1,0)),(0xf0,end),(0x144,velocity),(0x1b4,(0,1,0)),(0x1c0,normal)):
     u.mem_write(B+off,f(*val))
    u.mem_write(0x649f50,f(.5,.5,2500));u.mem_write(0x7c7058,f(0,-9.8,0))
    u.mem_write(S,w(STOP,B));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x4a01b0,STOP,count=150000)
    assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==S+4
    readw=lambda off:struct.unpack('<I',u.mem_read(B+off,4))[0]
    readf=lambda off,n=1:struct.unpack('<'+'f'*n,u.mem_read(B+off,4*n))
    assert readw(0x24)==3 and readw(0x2c)==0x10000 and readw(0x7c)==0 and readf(0x34)==(123.,)
    if fraction==1:
     assert trace==['0x4a01b0','0x49d280'] and readf(0x1b0)==(0.,),trace
    else:
     assert trace[:4]==['0x4a01b0','0x48a400','0x412b40','0x49d330'],trace
     assert readf(0x1b0)==(F(F(.1)-F(F(.1)*fraction)),)
     if elasticity==0:assert trace[-1]=='0x417e00' and not readw(0x1a8)&0x80000000,trace
    rows.append(dict(normal=normal,fraction=fraction,elasticity=elasticity,speed=speed,trace=trace,
                     flags=hex(readw(0x1a8)),remaining=readf(0x1b0)[0],position=readf(0xe4,3),velocity=readf(0x144,3)))
report=dict(result='PASS',original_sha256=SHA,cases=rows,scope='Complete4a01b0 with real48a400/412b40,49d330,417e00, pose/tensor/bounds and Default material getters. No callees substituted. Synthetic admitted dry kind3 contacts only; collision query, mover commit, whole scene scheduling and all other damage producers excluded. Object kind/handle/lifetime/flags remain intact; actor crush/damage route never entered.')
out=ROOT/'artifacts/geomod-postedit-re/fragment-ceiling-step.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(rows),'complete original fragment ceiling/floor contact steps')
