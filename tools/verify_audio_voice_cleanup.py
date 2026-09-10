"""Original counter-gated voice cleanup; hardware stop and bulk policy intercepted."""
import json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=runpy.run_path(str(root/'tools/verify_audio_registration.py'));m,call,u=env['m'],env['call'],env['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
trace=[]
def boundary(machine,address,size,user):
 sp=machine.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',machine.mem_read(sp,8));trace.append((address,arg))
 machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
for addr in (0x5442b0,0x5439b0):m.hook_add(UC_HOOK_CODE,boundary,begin=addr,end=addr)
results=[]
for enabled in (0,1):
 for keep in (0,1):
  m.mem_write(0x17543d8,u(enabled));trace.clear();want=[];expected=[]
  bank=bytearray(2600*64)
  for slot in range(55):bank[slot*64+56:slot*64+60]=u((-1,0,1,2,3)[slot%5]&0xffffffff)
  m.mem_write(0x1cd3ba8,bytes(bank))
  for family,count,start,stride in [(0,30,0x1753c38,44),(1,25,0x1754170,24)]:
   for i in range(count):
    slot=i+family*30;sample=-1 if i%7==0 else slot;handle=1000+slot
    words=list(range(100,100+stride//4));words[1-family]=sample&0xffffffff;words[family]=handle
    record=u(*words);m.mem_write(start+i*stride,record)
    if enabled and sample>=0 and (-1,0,1,2,3)[slot%5]<2:
     want.append((0x5442b0,handle));words[0]=words[1]=0xffffffff
     for field in ([2,4,5,9] if family==0 else [2,3,4,5]):words[field]=0
    expected.append((start+i*stride,u(*words)))
  call(0x5060b0,u(keep))
  if enabled:want.append((0x5439b0,keep))
  assert trace==want,(trace,want)
  for address,record in expected:assert bytes(m.mem_read(address,len(record)))==record,hex(address)
  assert bytes(m.mem_read(0x1cd3ba8,len(bank)))==bytes(bank)
  results.append(dict(enabled=enabled,keep=keep,stopped=sum(a==0x5442b0 for a,v in trace),slots=55))
report=dict(result='PASS',original_sha256=env['digest'],results=results,scope='Original5060b0 and voice reset callees unchanged, hardware stop5442b0 and downstream5439b0 intercepted. All55 voice slots and sample metadata bytes checked, with sentinel indices and counters below/at/above2. Does not verify bulk release timing, counter balancing or C/NXDK ownership.')
(root/'artifacts/audio-voice-cleanup-verification.json').write_text(json.dumps(report,indent=2));print(report)
