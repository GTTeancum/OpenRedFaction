"""Original kind3 contact dispatch and nonrigid response with varied counterpart metadata."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();B=0x30000000;S=B+0xe000;STOP=B+0xf000;STUB=B+0xf100
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rows=[]
for normal in [(0,1,0),(1,0,0),(.6,.8,0)]:
 baseline=None
 for inv in [0,.25,1]:
  for velocity in [(0,0,0),(4,-20,8),(-100,50,20)]:
   u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
   u.mem_write(STUB,b'\xd9\x05'+w(STUB+32)+b'\xc3');u.mem_write(STUB+16,b'\xd9\x05'+w(STUB+36)+b'\xc3');u.mem_write(STUB+32,f(.5,.5))
   for off,data in [(0x24,w(3)),(0x88,f(.6,0,.5)),(0x98,f(2)),(0xc0,f(1,0,0,0,1,0,0,0,1)),(0x144,f(-5,-5,0)),(0x1a8,w(0x8000003f)),(0x1b4,f(0,-.2,0)),(0x1c0,f(*normal)),(0x1d4,f(inv,*velocity)),(0x1e4,w(0x10000))]:u.mem_write(B+off,data)
   u.mem_write(B+0x2000,bytes([0x5a])*0x400);u.mem_write(B+0x2024,w(3));u.mem_write(B+0x202c,w(0x10000));u.mem_write(0x7394cc,w(B+0x2000));u.mem_write(0x7c7058,f(0,-9.8,0))
   counterpart=bytes(u.mem_read(B+0x2000,0x400));reads=[];calls=[]
   def code(cpu,a,n,d):
    if a in (0x48a400,0x412b40):calls.append(hex(a))
    if a==0x4687e0:cpu.reg_write(UC_X86_REG_EIP,STUB)
    elif a==0x468810:cpu.reg_write(UC_X86_REG_EIP,STUB+16)
   def read(cpu,access,a,n,value,d):
    if (a<B+0x1e8 and a+n>B+0x1d4) or (a<B+0x2400 and a+n>B+0x2000):reads.append(hex(a))
   u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_READ,read)
   def run(a):
    u.mem_write(S,w(STOP,B));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(a,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
   before=bytes(u.mem_read(B,0x400));run(0x48a400);assert u.reg_read(UC_X86_REG_EAX)==2 and calls==['0x48a400','0x412b40'];assert bytes(u.mem_read(B,0x400))==before
   run(0x49d330);assert not reads,reads;assert bytes(u.mem_read(B+0x2000,0x400))==counterpart
   result=bytes(u.mem_read(B,0x400));canonical=bytearray(result);canonical[0x1d4:0x1e8]=bytes(20)
   if baseline is None:baseline=canonical
   else:assert canonical==baseline
   rows.append(dict(normal=normal,counterpart_inverse_mass=inv,counterpart_velocity=velocity,dispatch=2,counterpart_reads=0,output_velocity=struct.unpack('<3f',result[0x144:0x150])))
report=dict(result='PASS',original_sha256=sha,cases=rows,scope='Real48a400/412b40 kind3 dispatch with liquid-transition marker zero, followed by full49d330 with material returns supplied. Counterpart metadata and target body are neither read nor mutated. Contact admission/query and earlier scheduling are NOT executed; this does not establish that every fragment pair is admitted.')
(ROOT/'artifacts/geomod-postedit-re/fragment-contact-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(rows),'admitted-contact response cases')
