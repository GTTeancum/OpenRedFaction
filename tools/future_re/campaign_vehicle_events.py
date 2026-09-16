"""Original cutscene completion broadcast and lifecycle boundary evidence; no port build."""
import sys,struct,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'local/python'))
(ROOT/'artifacts/future-campaign-re').mkdir(parents=True,exist_ok=True)
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
EXE=ROOT/'Installed_Game/RF.exe';SHA=hashlib.sha256(EXE.read_bytes()).hexdigest()
assert SHA=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
IMAGE=pefile.PE(str(EXE)).get_memory_mapped_image()
BASE=0x30000000;STACK=BASE+0x7e000;STOP=BASE+0x7f000
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
def machine():
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(IMAGE)+4095)//4096*4096);u.mem_write(0x400000,IMAGE);u.mem_map(BASE,0x80000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
def word(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def call(u,entry,args=(),ecx=0):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP

def return_boundary(u,value=0,pop=0):
 sp=u.reg_read(UC_X86_REG_ESP);ret=word(u,sp);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,ret)
results=[]
for kind,bit in [(77,0x400),(78,0x800)]:
 for present in [0,1]:
  for disabled in [0,1]:
   for empty in [0,1]:
    u=machine(); trace=[]; event=BASE; second=BASE+0x400; target=BASE+0x800; player=BASE+0x2000; mover=BASE+0x4000
    u.mem_write(0x5cb054,w(player if present else 0));u.mem_write(0x7c75d4,w(player));u.mem_write(player+0x10,w(0x55|0xc00));u.mem_write(mover+0x2c,w(333))
    for obj in [event,second,target]:
     u.mem_write(obj,w(0x589c9c));u.mem_write(obj+0x290,w(83 if obj==target else kind,0,0xffffffff,0,0,0))
    u.mem_write(event+0x2b0,w(disabled));u.mem_write(event+0x29c,w(0 if empty else 3,3,BASE+0x6000));u.mem_write(BASE+0x6000,w(101,102,103))
    u.mem_write(second+0x29c,w(1,1,BASE+0x6010));u.mem_write(BASE+0x6010,w(101))
    u.mem_write(target+0x29c,w(1,1,BASE+0x6020));u.mem_write(BASE+0x6020,w(777))
    def hook(cpu,a,n,data):
     sp=cpu.reg_read(UC_X86_REG_ESP)
     if a in [0x4b6800,0x46afa0]:
      handle=word(cpu,sp+4);trace.append([hex(a),handle]);return_boundary(cpu,target if a==0x4b6800 and handle==101 else mover if a==0x46afa0 and handle==102 else 0)
     elif a in [0x4b65c0,0x46aba0]:
      trace.append([hex(a),*struct.unpack('<3I',cpu.mem_read(sp+4,12))]);return_boundary(cpu)
    u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b8ce0,ecx=event);first=trace.copy();call(u,0x4b8ce0,ecx=second)
    assert trace==first # first monitor consumes pulse, including disabled / empty monitors
    expected=(0x55|0xc00)&~bit if present else 0x55|0xc00
    assert word(u,player+0x10)==expected
    assert bool(first)==bool(present and not empty)
    if first:assert ['0x4b65c0',777,0xffffffff,0xffffffff] in first and ['0x46aba0',333,0xffffffff,0xffffffff] in first
    results.append(dict(kind=kind,player_present=present,monitor_disabled=disabled,empty_links=empty,pulse_after=hex(word(u,player+0x10)),trace=first))
for initial in [0,0x80,0xabcdef7f,0xffffffff]:
 u=machine();event=BASE; entity=BASE+0x2000;u.mem_write(event,w(0x589c9c));u.mem_write(event+0x290,w(80));u.mem_write(event+0x29c,w(2,2,BASE+0x6000));u.mem_write(BASE+0x6000,w(101,999));u.mem_write(entity+0x814,w(initial))
 def hook(cpu,a,n,data):
  if a==0x426fc0:return_boundary(cpu,entity if word(cpu,cpu.reg_read(UC_X86_REG_ESP)+4)==101 else 0)
 u.hook_add(UC_HOOK_CODE,hook);call(u,0x4b9070,ecx=event);on=word(u,entity+0x814);assert on==initial|0x80
 call(u,0x4b9f80,ecx=event);off=word(u,entity+0x814);assert off==initial&~0x80
 results.append(dict(kind=80,initial=hex(initial),on=hex(on),off=hex(off)))
report=dict(result='PASS',original_sha256=SHA,cases=len(results),results=results,limitations=['Event/entity/mover resolution and outgoing mover/event effects intercepted; actual monitor, array, activation, no-op83, propagation and on/off dispatcher executed.','Does not execute vehicle boarding/physics.'])
(ROOT/'artifacts/future-campaign-re/vehicle-events.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(results),'vehicle event cases')
data=json.loads((ROOT/'artifacts/events.json').read_text());authored=[dict(file=level['file'],**event) for level in data['results'] for event in level['records'] if event['type_index'] in [77,78,80]]
(ROOT/'artifacts/future-campaign-re/vehicle-events-authored.json').write_text(json.dumps(authored,indent=2)+'\n');print('authored',len(authored))
