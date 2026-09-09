"""Original actor parameter prefix and positive-mass body preparation, no hooks."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;cls=base+0x2000;orientation=base+0x4000;position=base+0x5000;params=base+0x6000;stack=base+0xd000;stop=base+0xf000
u.mem_map(base,0x10000);u.mem_map(0,4096)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
f=lambda *v:struct.pack('<%df'%len(v),*v)
count=0
for mass in (1,10,100,1000):
 for angle in range(4):
  for flags,source_count in ((0,0),(0x80000070,0),(0x80000078,3)):
   matrix=[1,0,0,0,1,0,0,0,1] if angle%2==0 else [0,0,1,0,1,0,-1,0,0]
   xyz=[angle*2,-3,17]
   u.mem_write(base,bytes(0xe000));u.mem_write(cls+8,w(5,cls+0x100));u.mem_write(cls+0x100,b'miner\0')
   u.mem_write(cls+0x68,f(mass));u.mem_write(cls+0x94,w(9,3));u.mem_write(orientation,f(*matrix));u.mem_write(position,f(*xyz))
   u.mem_write(stack+0x144,w(position));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,stack+0x90)
   u.reg_write(UC_X86_REG_EBX,cls);u.reg_write(UC_X86_REG_EBP,0);u.reg_write(UC_X86_REG_ESI,orientation);u.reg_write(UC_X86_REG_FPCW,0x37f)
   u.emu_start(0x42254d,0x4225e9,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4225e9
   p=bytearray(u.mem_read(stack+0x90,0x98));assert p[0x18:0x3c]==bytes(36)
   assert p[0x14:0x18]==f(mass) and p[0x3c:0x48]==f(*xyz) and p[0x48:0x6c]==f(*matrix)
   p[0x94:0x98]=w(flags);p[0x84:0x88]=f(1);p[0x88:0x94]=w(source_count,16,base+0x7000)
   u.mem_write(base+0x7000,(f(0,0,0,1,-1)+w(0))*3)
   u.mem_write(params,bytes(p));u.mem_write(base,bytes(0x170));u.mem_write(base+0xfc,w(0,16,base+0x8000));u.mem_write(stack,w(stop,base,params))
   u.reg_write(UC_X86_REG_ESP,stack)
   try:u.emu_start(0x49ec90,stop,count=100000)
   except Exception:
    print(mass,angle,hex(flags),hex(u.reg_read(UC_X86_REG_EIP)),p.hex());raise
   assert u.reg_read(UC_X86_REG_EIP)==stop
   tensor=f(1,0,0,0,1,0,0,0,1) if flags&0x70 and source_count==0 else bytes(36)
   assert bytes(u.mem_read(base+0x14,72))==tensor*2 and bytes(u.mem_read(base+0x10,4))==f(mass)
   assert bytes(u.mem_read(params+0x18,36))==tensor;count+=1
report=dict(result='PASS',original_cases=count,scope='Original 42254d..4225e9 parameter initialization followed by complete 49ec90 positive-mass preparation with empty and populated sphere lists in preallocated storage. No replaced callees; allocation/registration and initial model pose excluded.')
if len(sys.argv)>1:
 s=json.loads(Path(sys.argv[1]).read_text())['symbols']
 for name in ('rf_scene_actor_initial_state','scene_actor_body'):
  assert s[name]['words'][4:22]==[0]*18,(name,'tensor differs')
 report['guest_initial_and_final_tensor_bytes']=144
(root/'artifacts/actor-initial-tensor-verification.json').write_text(json.dumps(report,indent=2));print(report)
