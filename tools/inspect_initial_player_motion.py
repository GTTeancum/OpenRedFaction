"""Execute the original first selector with explicitly sourced creation fields.
This is a creation-field fixture, not execution of the complete spawn routine.
"""
import hashlib,itertools,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
entity,info,mode,stack,stop=[0x30000000+i*0x100000 for i in range(5)]
for a in (entity,info,mode,stack,stop):u.mem_map(a,65536)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
trace=[]
def observe(uc,a,n,data):
 trace.append(a)
 if len(trace)>40:del trace[0]
u.hook_add(UC_HOOK_CODE,observe)
results=[]
obj,desc,motions,data=[0x30500000+i*0x100000 for i in range(4)]
for a in (obj,desc,motions,data):u.mem_map(a,65536)
initial=struct.pack('<iiffiI',0,-1,0,0,0,0)
empty=struct.pack('<I',0)+bytes(192)+struct.pack('<3i',-1,-1,-1)+bytes(40)+struct.pack('<fII',0,1,0)
resources=[struct.pack('<f4iI3i',1,0,9600,0,0,1,0,0,0) for _ in range(32)]
mapping=list(range(23))
delta=struct.unpack('<f',struct.pack('<f',1/30))[0]
probe=str(root/'build/pc/Release/rf_motion_probe.exe')
def call(address,end=stop):
 u.reg_write(UC_X86_REG_FPCW,0x37f)
 try:u.emu_start(address,end,count=100000)
 except Exception:
  print('trace',[hex(a) for a in trace]);raise
 assert u.reg_read(UC_X86_REG_EIP)==end
for player,primary,secondary in itertools.product((0,1),(-1,5),(-1,6)):
 u.mem_write(entity,bytes(65536));u.mem_write(info,bytes(65536));u.mem_write(mode,bytes(65536))
 # 422360 param6 bit1 -> generic object flag8; 4a41bf supplies one.
 put(entity+0x7c,'<I',8 if player else 0)
 put(entity+0x24,'<I',0);put(entity+0x2c,'<i',5)
 put(entity+0x200,'<i',-1);put(entity+0x294,'<I',info);put(entity+0x29c,'<I',info)
 put(info+0x94,'<i',2);put(info+0x1b4,'<i',0)
 # miner1 authored run mode, assigned by 422df5/422e03.
 put(entity+0x858,'<I',mode);put(mode+4,'<i',1)
 # 402c20 writes action+520 and behavior+554 to zero; default primary assigned later.
 put(entity+0x2a0,'<I',entity);put(entity+0x2a4,'<ii',primary,secondary)
 # Execute the original initializer's scalar-write span with poisoned targets.
 put(entity+0x520,'<I',0xa5a5a5a5);put(entity+0x554,'<I',0xa5a5a5a5)
 u.reg_write(UC_X86_REG_ESI,entity+0x2a0);u.reg_write(UC_X86_REG_EBX,0)
 call(0x402d68,0x402dad)
 assert bytes(u.mem_read(entity+0x520,4))==bytes(4)
 assert bytes(u.mem_read(entity+0x554,4))==bytes(4)
 put(entity+0x834,'<i',-1);put(entity+0x1380,'<i',-1)
 put(entity+0x138c,'<iiff',0,-1,0,0)
 put(entity+0x8c0,'<f',6)
 for i in range(23):put(entity+0x8e4+16*i,'<i',i)
 for i in range(45):put(entity+0xa54+16*i,'<i',-1)
 put(0x7c75cc,'<I',0);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
 # Materialize the original loaded-character control layout (no pose sampling).
 u.mem_write(obj,bytes(65536));u.mem_write(desc,bytes(65536))
 put(obj+0x1d50,'<I',desc);put(obj+0x1cfc,'<ii',-1,-1);put(obj+0x1d48,'<i',-1)
 put(obj+0x1cf8,'<H',1);put(desc+0xf58,'<I',32)
 for i in range(32):
  m=motions+i*256;d=data+i*256
  put(desc+0xf5c+4*i,'<I',m);put(m+0x78,'<I',d);put(m+0x74,'<I',0)
  u.mem_write(desc+0x120c+i,b'\x01');put(d+16,'<ii',0,9600)
 wrapper=obj+0x4000;put(wrapper,'<II',2,obj);put(entity+0x80,'<I',wrapper)
 put(0x5a4014,'<f',delta)
 put(stack+64000,'<II',stop,entity);u.reg_write(UC_X86_REG_ESP,stack+64000)
 # Entire selector, gates, transition update and motion-weight assignment execute.
 call(0x41f270)
 armed=primary!=-1 or secondary!=-1
 movement=struct.pack('<3f5i',0,0,0,1,0,int(player and armed),3 if player and armed else 2,5 if player and armed else 4)
 selected=subprocess.check_output([probe,'--movement'],input=initial+struct.pack('<23i',*mapping)+movement)
 assert selected[:4]==bytes(4)
 actual=subprocess.check_output([probe,'--controller'],input=empty+b''.join(resources)+selected[4:]+struct.pack('<23if',*mapping,delta))
 read=lambda a,n:bytes(u.mem_read(a,n))
 state=read(obj+0x12d0,196)+read(obj+0x1cfc,8)+read(obj+0x1d48,4)+struct.pack('<II',read(obj+0x1d4c,1)[0],read(obj+0x1d14,1)[0])+read(obj+0x1d18,32)+read(obj+0x1d04,4)+struct.pack('<II',struct.unpack('<H',read(obj+0x1cf8,2))[0],read(obj+0x1d44,1)[0]|read(obj+0x1d45,1)[0]<<1)
 expected=bytes(4)+state+read(entity+0x138c,16)+initial[16:]+b''.join(read(motions+i*256+0x74,4) for i in range(32))
 assert actual==expected,(player,primary,secondary,[i for i in range(0,416,4) if actual[i:i+4]!=expected[i:i+4]])
 controller=struct.unpack('<iiff',read(entity+0x138c,16))
 assert controller[:2]==(0,1 if player and armed else -1)
 count=struct.unpack_from('<I',state)[0]
 results.append(dict(player_flag=player,primary=primary,secondary=secondary,controller=controller,
                     slots=[struct.unpack_from('<iif',state,4+12*i) for i in range(count)]))
report=dict(result='PASS',cases=len(results),results=results,
 scope='Original complete 41f270 and unmodified callees against PC movement/controller composition; creation-field fixtures, not complete 422360/4a4130 or loaded model pose. Scalar action/behavior defaults execute 402d68..402dac; remaining fields explicitly materialized.')
(root/'artifacts/initial-player-motion.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
