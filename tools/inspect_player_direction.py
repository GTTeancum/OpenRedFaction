"""Execute original movement-action composition before look input processing.

Only the hardware/action-value reader 43d390 is supplied by a fixture. The
writer instructions execute unchanged; this does not execute OS input APIs.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536)
player,actions,output,stub,value,stack,stop=[base+n for n in (0,0x2000,0x4000,0x8000,0x9000,0xe000,0xf000)]
pack=lambda v:struct.pack('<'+'I'*len(v),*v)
f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
u.mem_write(stub,b'\xd9\x05'+pack([value])+b'\xc3') # fixture float return
requests=[];levels={}
def observe(cpu,address,size,data):
    if address!=0x43d390:return
    sp=cpu.reg_read(UC_X86_REG_ESP);control,index=struct.unpack('<II',cpu.mem_read(sp+4,8))
    assert control==actions
    requests.append(index);cpu.mem_write(value,struct.pack('<f',levels[index]));cpu.reg_write(UC_X86_REG_EIP,stub)
u.hook_add(UC_HOOK_CODE,observe)
rng=random.Random(0x4307a0);records=[]
for upward in (0,1):
 for downward in (0,1):
  for index in range(128):
   levels={a:f32(rng.random()) for a in (14,13,15,16,11,12,3,4)}
   if index==0:levels={a:float(a in (14,11)) for a in levels}
   before=bytes([0xa5]*64);u.mem_write(output,before)
   u.mem_write(stack,pack([stop,player,actions,output,upward,downward]));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);requests.clear()
   u.emu_start(0x4307a0,0x43083b,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x43083b
   expected=[f32(levels[14]-levels[13]),f32(levels[15]-levels[16]),f32(levels[11]-levels[12])]
   if upward:expected[1]=f32(expected[1]+levels[3])
   if downward:expected[1]=f32(expected[1]-levels[4])
   after=bytes(u.mem_read(output,64));assert after[12:24]==struct.pack('<3f',*expected)
   assert before[:12]==after[:12] and before[24:]==after[24:]
   assert requests==[14,13,15,16,11,12]+([3] if upward else [])+([4] if downward else [])
   records.append(dict(upward=upward,downward=downward,actions=levels,direction=expected))
u.mem_write(output,bytes([0xa5]*64));u.mem_write(stack,pack([stop,output]));u.reg_write(UC_X86_REG_ESP,stack)
u.emu_start(0x430fc0,stop,count=1000);after=bytes(u.mem_read(output,64))
assert after[:24]==bytes(24) and after[24:28]==bytes([0xa5]*4) and after[28:36]==bytes(8) and after[36:]==bytes([0xa5]*28)
report=dict(result='PASS',original_sha256=digest,cases=len(records),clear_case=True,
 scope='Original 4307a0..430838 with action-value reader fixture, and complete 430fc0. Direction is raw action differences plus gated vertical actions, without normalization; look/input ownership, locking gates and collision ordering outside this execution.',records=records)
(root/'artifacts/player-direction-reference.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='records'},indent=2))
