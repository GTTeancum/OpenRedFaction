"""Execute original generic mover-relative query preparation, stopping at geometry."""
import hashlib, json, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
SHA='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
im=pefile.PE(str(exe)).get_memory_mapped_image()
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;M=B+0x1000;SPHERES=B+0x2000;STACK=B+0xe000;STOP=B+0xf000
identity=[1,0,0,0,1,0,0,0,1]
rows=[]
for rotated in [False,True]:
 for ratio in [0.25,0.5,1.0]:
  for motion in [(0,0,0),(4,0,0),(0,-2,1)]:
   for center in [(0,0,0),(.5,-.25,.125)]:
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
    word=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
    floats=lambda a,n:list(struct.unpack('<'+'f'*n,u.mem_read(a,n*4)))
    start=[5,4,3];end=[6,2,4];mstart=[1,2,1];mend=[a+b for a,b in zip(mstart,motion)]
    for obj,pos,nxt in [(B,start,end),(M,mstart,mend)]:
     for off,value in [(0xe4,pos),(0xf0,nxt),(0xfc,identity),(0x120,identity),(0x48,identity),(0x190,[-100]*3),(0x19c,[100]*3)]:u.mem_write(obj+off,f(*value))
    u.mem_write(B+0x24,w(3));u.mem_write(B+0x1a8,w(0x8000003f));u.mem_write(B+0x180,f(1));u.mem_write(B+0x1cc,f(1));u.mem_write(B+0x1b0,f(ratio))
    u.mem_write(M+0x1b0,f(1));u.mem_write(M+0x28c,w(0x64e6e0));u.mem_write(0x64e96c,w(M))
    u.mem_write(B+0x184,w(1,1,SPHERES));u.mem_write(SPHERES,f(*center,.125,-1)+w(0))
    if rotated:
     u.mem_write(M+0x48,f(0,1,0,-1,0,0,0,0,1))
    captured=[]
    def ret():
     sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp))
    def code(cpu,a,n,data):
     if a==0x5735cd:ret()
     elif a==0x4df1c0:
      sp=cpu.reg_read(UC_X86_REG_ESP);q=word(sp+4)
      captured.append(dict(start=floats(q+0x34,3),delta=floats(q+0x40,3),radius=floats(q+0x4c,1)[0],flags=word(q+0x50)))
      cpu.emu_stop()
    u.hook_add(UC_HOOK_CODE,code);u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x49bb70,STOP,count=100000)
    assert len(captured)==1,hex(u.reg_read(UC_X86_REG_EIP))
    origin=[a+(1-ratio)*d for a,d in zip(mstart,motion)]
    expected_start=[a+c-m for a,c,m in zip(start,center,origin)]
    expected_end=[a+c-m for a,c,m in zip(end,center,mend)]
    if rotated:
     expected_start=[expected_start[1],-expected_start[0],expected_start[2]]
     expected_end=[expected_end[1],-expected_end[0],expected_end[2]]
    expected=dict(start=expected_start,delta=[b-a for a,b in zip(expected_start,expected_end)],radius=.125,flags=4)
    assert captured[0]==expected,(ratio,motion,center,captured,expected)
    rows.append(dict(rotated=rotated,ratio=ratio,motion=motion,center=center,query=captured[0]))
out=ROOT/'artifacts/geomod-postedit-re/fragment-mover-motion.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(dict(result='PASS',original_sha256=SHA,cases=rows,scope='Original49bb70 through first mover geometry call; only atexit registration stubbed. Identity body poses, identity/quarter-turn mover poses, one sphere, overlapping synthetic bounds. Stops before geometry, response, publication or scheduling.'),indent=2)+'\n')
print('PASS',len(rows),'original mover-relative query preparations')
