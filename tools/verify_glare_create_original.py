"""Execute full original4104a0 with explicit resource/allocation boundaries.

Synthetic class/owner views verify orchestration, not the supplied services.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=ROOT/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x200000);u.mem_map(0,4096)
OBJ,NAME,POS,MATRIX,STACK,STOP=B+0x1000,B+0x3000,B+0x4000,B+0x4100,B+0xe000,B+0xf000
CLS=0x5afb88;SENTINEL=0x5c9360
w=lambda *a:struct.pack('<%dI'%len(a),*(v&0xffffffff for v in a))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cstr(a):
    result=bytearray()
    while u.mem_read(a,1)!=b'\0':result+=u.mem_read(a,1);a+=1
    return bytes(result)
def string(a,value):
    global pool
    u.mem_write(pool,value+b'\0');u.mem_write(a,w(len(value),pool));pool+=len(value)+1

from unicorn.x86_const import UC_X86_REG_FPCW
PARENT=B+0x6000;NODE=B+0x7000;RADIUS=B+0x8000;STUB=B+0x8100
CLASS=0x5c9e98;SENT=0x5c9ba8
u.mem_write(STUB,b'\xd9\x05'+w(RADIUS)+b'\xc3')
trace=[];fail=False
position=struct.pack('<3f',1.25,-2.5,3.75);matrix=struct.pack('<9f',1,0,0,0,1,0,0,0,1)
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);result=0
 if address==0x40a4a0:
  trace.append(('radius',arg(0),arg(1)));cpu.reg_write(UC_X86_REG_EIP,STUB);return
 if address==0x40a0e0:
  trace.append(('parent',arg(0)));result=PARENT
 elif address==0x5034f0:
  trace.append(('tag',arg(0),arg(1)));assert arg(2)==PARENT+0x48 and arg(3)==PARENT+0x3c
  cpu.mem_write(arg(4),matrix);cpu.mem_write(arg(5),position)
 elif address==0x486da0:
  trace.append(('allocate',arg(0),arg(1),arg(2),arg(4),arg(5)))
  desc=arg(3);assert bytes(cpu.mem_read(desc+0x3c,12))==position and bytes(cpu.mem_read(desc+0x48,36))==matrix
  assert r(desc+0x14)==0x3f800000 and r(desc+0x84)==r(RADIUS) and r(desc+4)==0
  result=0 if fail else OBJ
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,r(sp))
for a in (0x40a4a0,0x40a0e0,0x5034f0,0x486da0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
cases=0
for cls in (-1,0,1,2,3):
 for flag in (0,1,2,255,256,257):
  for fail in (False,True):
   trace=[];u.mem_write(OBJ,b'\xa5'*0x300);before=bytes(u.mem_read(OBJ,0x300));u.mem_write(0x5cab98,w(3));u.mem_write(0x5c9e64,w(NODE));u.mem_write(NODE+0x2b8,w(SENT))
   u.mem_write(PARENT+0x80,w(987));u.mem_write(RADIUS,struct.pack('<f',.75))
   for i in range(3):u.mem_write(CLASS+i*52+40,struct.pack('<2f',.5,1.0))
   u.mem_write(STACK,w(STOP,123,7,cls,flag));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f)
   u.emu_start(0x413d20,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
   actual=bytes(u.mem_read(OBJ,0x300));expected=bytearray(before)
   if cls<0 or cls>=3:
    assert not trace and u.reg_read(UC_X86_REG_EAX)==0 and actual==before
   else:
    assert trace==[('radius',0x3f000000,0x3f800000),('parent',123),('tag',987,7),('allocate',10,0xffffffff,123,0x30000,0)],trace
    if fail:assert actual==before and u.reg_read(UC_X86_REG_EAX)==0 and r(0x5c9e64)==NODE
    else:
     expected[0x200:0x208]=w(123,7);expected[0x28c]=1;expected[0x290:0x2ac]=w(-1,0,0,0,0,0,0)
     expected[0x2ac:0x2c0]=w(CLASS+cls*52,cls,2 if flag&255 else 0,SENT,NODE)
     expected[0x2c0:0x2cc]=struct.pack('<3f',-1000,-1000,-1000);expected[0x2cc:0x2d0]=w(0);expected[0x2d0]=0;expected[0x2d4:0x2ec]=bytes(24)
     assert actual==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
     assert u.reg_read(UC_X86_REG_EAX)==OBJ and r(NODE+0x2b8)==OBJ and r(0x5c9e64)==OBJ
   cases+=1
report=dict(result='PASS',cases=cases,scope='Full original413d20 with actual descriptor/vector initialization and destruction; supplied random-radius, parent lookup, tag pose and generic allocation boundaries. Complete object write footprint, ordered calls, descriptor fields, low-byte flag, class bounds, allocation failure and tail linkage. No shared constructor, resource lifetime or native Xbox claim.')
(ROOT/'artifacts/glare-create-original.json').write_text(json.dumps(report,indent=2));print(report)
