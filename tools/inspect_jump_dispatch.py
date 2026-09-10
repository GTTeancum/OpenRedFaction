"""Execute original action-3 dispatcher with scoped object-resolution hooks."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
binary=root/'Installed_Game/RF.exe';sha=hashlib.sha256(binary.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(binary));im=p.get_memory_mapped_image();p.close();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);player=base;entity=base+0x2000;control=base+0x4000;parent=base+0x5000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
calls=[]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if address==0x426fc0:
  handle=read(sp+4);assert handle in (101,202);value=(entity if present else 0) if handle==101 else (parent if parent_kind!=-1 else 0)
 elif address==0x40a0e0:assert read(sp+4)==303;value=control if control_kind!=-1 else 0
 elif address==0x486c90:
  obj=cpu.reg_read(UC_X86_REG_ECX);assert obj in (parent,control);value=parent_kind if obj==parent else control_kind
 elif address==0x434200:value=game_state
 else:assert read(sp+4)==entity;calls.append('jump');value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (0x426fc0,0x40a0e0,0x486c90,0x434200,0x4288b0):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
results=[]
for present,override,game_state,control_kind,parent_kind,flag,key,edge in itertools.product((0,1),(0,1,2),(0,34),(-1,0,5),(-1,1,4),(0,1),(0,1),(0,1)):
 seed=bytearray(0x4000);struct.pack_into('<I',seed,0x14,101);struct.pack_into('<I',seed,0xb4,303);struct.pack_into('<I',seed,0x2200,202);struct.pack_into('<I',seed,0x2810,0x80000000|flag)
 u.mem_write(base,bytes(seed));u.mem_write(0x64ecb9,bytes([override]));u.mem_write(0x18868f4+0x29,bytes([key]));u.mem_write(stack,w(stop,player,3,edge));u.reg_write(UC_X86_REG_ESP,stack);calls.clear()
 u.emu_start(0x4a6210,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 expected=bool(present and not(override and game_state==34) and control_kind!=5 and parent_kind!=4 and not flag and not key)
 assert bool(calls)==expected and len(calls)<=1
 assert bytes(u.mem_read(base,len(seed)))==seed
 results.append(dict(present=present,override=override,game_state=game_state,control_kind=control_kind,parent_kind=parent_kind,actor_flags=0x80000000|flag,key=key,edge=edge,dispatched=expected))
report=dict(result='PASS',cases=len(results),original_sha256=sha,scope='Original action-3 dispatcher and 4a5c00 gate. Object lookup/type, game-state query and jump boundary supplied; action query/physical input not executed.',results=results)
(root/'artifacts/jump-dispatch.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'original jump dispatch cases')
