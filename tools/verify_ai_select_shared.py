"""Compare shared408ac0 dispatcher against the full original trace oracle."""
import json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
subprocess.run([sys.executable,str(root/'tools/verify_ai_select_original.py')],check=True)
records=json.loads((root/'artifacts/ai-select-original.json').read_text())['records']
ops=(0x427020,0x4174c0,0x4087a0,0x408dc0,0x408ef0,0x427fb0,0x42a020,0x40a110,0x40a210,0x408d90,0x406b70,0x40ac90,0x4062c0)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
fb=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
fields=(0x4d4,0x520,0x528,0x52c,0x530,0x554,0x55c,0x7d0,0x810,0x834)
commands=[];expected=[]
for rec in records:
 cfg=rec['input'];initial=[rec['initial_fields'][hex(o)] for o in fields]
 commands.append(w(*initial,*(cfg[hex(op)] for op in ops),cfg['owner'],cfg['target'],cfg['changed_state'],fb(cfg['threshold']),cfg['network'],rec['case'],*(fb(v) for v in cfg['positions'])))
 trace=[]
 for event in rec['trace']:
  op=int(event[0],16)
  if op in (0x407e20,0x407e80,0x4fa3b0):continue
  trace.extend((op,event[1] if op in ops or op==0x426fc0 else 0))
 expected.append(w(0,*(rec['fields'][hex(o)] for o in fields),*rec['notices'],rec['rng'],len(trace)//2,*trace,*([0]*(128-len(trace)))))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ai-select'],input=b''.join(commands))
assert len(actual)==len(records)*576
for i,want in enumerate(expected):assert actual[i*576:(i+1)*576]==want,('PC',i)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x10000);S=B;F=B+0x1000;BE=F+0x100;R=F+0x200;P=F+0x300;STACK=B+0xe000;STOP=B+0xf000;LOOK=STOP+0x100;CALL=STOP+0x200
entry=int(re.search(r'\s_rf_entity_ai_select\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
trace=[];cfg={};failure=-1;calls=0
actors=[S+i*0x100 for i in range(5)]
def hook(cpu,address,size,context):
 global calls
 if address not in (LOOK,CALL):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4);status=0;calls+=1
 if calls==failure:status=0xffffffff
 elif address==LOOK:
  handle=arg(1);trace.extend((0x426fc0,handle));assert handle in (55,77)
  cpu.mem_write(arg(2),w((S if cfg['owner'] else 0) if handle==55 else (actors[4] if cfg['target'] else 0)))
 else:
  assert arg(1)==S;op=arg(3);value=cfg[hex(op)] if op in ops else 0;trace.extend((op,value))
  assert arg(2)==S
  if op==0x401060:cpu.mem_write(arg(4),struct.pack('<3f',2,3,4))
  if op==0x40ac90:assert bytes(cpu.mem_read(arg(4),12))==struct.pack('<3f',2,3,4)
  if op==0x4065d0:cpu.mem_write(S+16,w(cfg['changed_state']))
  if op==0x401cc0:cpu.mem_write(arg(6),struct.pack('<d',cfg['threshold']))
  cpu.mem_write(arg(5),w(value))
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
x.hook_add(UC_HOOK_CODE,hook)
def setup(rec):
 global cfg,trace,calls
 cfg=rec['input'];v=[rec['initial_fields'][hex(o)] for o in fields];x.mem_write(B,bytes(0x2000))
 x.mem_write(S,w(v[1],v[2],v[3],v[4],v[5],v[6],v[7],S,55,7,v[8],v[9],77,v[0],0,0,0))
 for i in range(1,4):x.mem_write(actors[i],w(2 if i%2 else 4));x.mem_write(actors[i]+36,w(7 if i<3 else 8))
 x.mem_write(S+56,struct.pack('<3f',*cfg['positions'][:3]));x.mem_write(actors[4]+56,struct.pack('<3f',*cfg['positions'][3:]));x.mem_write(R,w(rec['case']));x.mem_write(P,w(*actors[:4]))
 x.mem_write(F,w(fb(12345.75),12345,cfg['network']&1,cfg['network']>>1,R,P,4));x.mem_write(BE,w(LOOK,CALL,0));trace=[];calls=0
 x.mem_write(STACK,w(STOP,S,F,BE));x.reg_write(UC_X86_REG_ESP,STACK)
def run():
 x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
for i,rec in enumerate(records):
 setup(rec);status=run();values=[r(S+o) for o in (52,0,4,8,12,16,20,24,40,44)]
 result=w(status,*values,*(r(a+24) for a in actors[1:4]),r(R),len(trace)//2,*trace,*([0]*(128-len(trace))))
 assert result==expected[i],('NXDK',i)
# Every callback boundary in the longest path must propagate failure and stop.
rec=max(records,key=lambda z:len(z['trace']));setup(rec);run();total=calls
for failure in range(1,total+1):
 setup(rec);assert run()==0xffffffff and calls==failure
failure=-1
print(dict(result='PASS',original_pc_nxdk_cases=len(records),callback_failure_cases=total,scope='Shared full408ac0 exact projected fields, ordered external calls, peer writes and RNG vs original. External AI services supplied; no native scene scheduling claim.'))
(root/'artifacts/ai-select-shared.json').write_text(json.dumps(dict(result='PASS',cases=len(records),callback_failures=total),indent=2))
