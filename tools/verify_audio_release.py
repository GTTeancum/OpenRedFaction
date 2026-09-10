"""Original sample release preserves registration metadata; device free intercepted."""
import itertools,json,runpy,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=runpy.run_path(str(root/'tools/verify_audio_registration.py'))
m,call,u=env['m'],env['call'],env['u']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
freed=[]
def release(machine,address,size,user):
 sp=machine.reg_read(UC_X86_REG_ESP);ret,handle=struct.unpack('<2I',machine.mem_read(sp,8));freed.append(handle)
 machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,ret)
m.hook_add(UC_HOOK_CODE,release,begin=0x522270,end=0x522270)
rows=[]
for enabled,backend,handle,retained,index in itertools.product([0,1],[1,2],[-1,0,17],[0,1],[-1,0]):
 record=bytearray(range(64));record[:16]=b'release_test.wav';record[15]=0
 record[48:52]=u(handle&0xffffffff);record[61]=1;record[63]=retained
 m.mem_write(0x1cd3ba8,bytes(record));m.mem_write(0x1cfc5cc,u(1));m.mem_write(0x1cfc5d0,u(enabled,backend));freed.clear()
 call(0x543930,u(index&0xffffffff))
 expected=bytearray(record);active=enabled and index!=-1 and not retained and handle>=0
 if active:expected[48:52]=u(0xffffffff);expected[61]=0
 assert bytes(m.mem_read(0x1cd3ba8,64))==bytes(expected)
 assert freed==([handle] if active and backend==1 else [])
 assert bytes(m.mem_read(0x1cfc5cc,4))==u(1)
 rows.append(dict(enabled=enabled,backend=backend,handle=handle,retained=retained,index=index,released=bool(active)))
# Bulk release traverses all2600 slots, including slots past the registration count.
bank=bytearray(2600*64)
for slot in range(2600):bank[slot*64+48:slot*64+52]=u(0xffffffff)
for slot in (0,88,2599):
 bank[slot*64:slot*64+4]=b'pcm\0';bank[slot*64+48:slot*64+52]=u(slot+100);bank[slot*64+61]=1
bank[88*64+63]=1
m.mem_write(0x1cd3ba8,bytes(bank));m.mem_write(0x1cfc5cc,u(88));m.mem_write(0x1cfc5d0,u(1,1));freed.clear()
call(0x543980,b'')
for slot in (0,2599):bank[slot*64+48:slot*64+52]=u(0xffffffff);bank[slot*64+61]=0
assert freed==[100,2699] and bytes(m.mem_read(0x1cd3ba8,len(bank)))==bytes(bank)
assert bytes(m.mem_read(0x1cfc5cc,4))==u(88)
report=dict(result='PASS',cases=len(rows),bulk_slots=2600,original_sha256=env['digest'],scope='Original543930 with only final device release522270 intercepted. Exact64-byte record and registry count checked across enabled/backend/handle/retention/sentinel cases. Bulk543980 also scans2600 slots while preserving registration count and retained slots. No level-transition scheduling, actual PCM free or C/NXDK implementation claim.',rows=rows)
(root/'artifacts/audio-release-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})
