"""Original fixed pool layout and emitter-particle detachment recovery."""
import runpy,struct,json
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_creation.py')))
u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];root=ctx['root'];put=ctx['put'];get=ctx['get']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX

def call(address,this=0):
 u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,this);u.emu_start(address,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop

def chain(sentinel,nodes):
 for i,a in enumerate([sentinel]+nodes):
  put(a,([sentinel]+nodes)[(i+1)%(len(nodes)+1)]);put(a+4,([sentinel]+nodes)[i-1])

def verify_chain(sentinel,nodes):
 addresses=[sentinel]+nodes
 for i,a in enumerate(addresses):assert get(a)==addresses[(i+1)%len(addresses)] and get(a+4)==addresses[i-1],hex(a)

# Execute original initialization against poisoned storage, including adjacent guards.
for start,count in ((0x7a3cf8,500),(0x782748,1100)):u.mem_write(start,bytes([0xa5])*(count*120))
call(0x494e70)
for kind,start,count in ((0,0x7a3cf8,500),(1,0x782748,1100)):
 desc=0x7a3b00+kind*0xfc
 assert get(desc)==start and get(desc+4)==count and get(desc+0xf8)==0
 verify_chain(desc+8,[start+i*120 for i in range(count)]);verify_chain(desc+0x80,[])
 for i in range(count):
  record=bytearray([0xa5]*120);node=start+i*120
  record[:8]=u.mem_read(node,8);record[0x58:0x5c]=bytes(4);record[0x4e:0x50]=bytes(2)
  assert bytes(u.mem_read(node,120))==record

# Detach preserves live particle data and pool counts, appending in source order.
owner=base+0x4000;destination=0x7bd670;cases=0
for count in (0,1,2,8,31):
 for existing in (0,1,3):
  nodes=[base+0x2000+i*120 for i in range(count)]
  old=[base+0x6000+i*120 for i in range(existing)]
  for a in nodes+old:u.mem_write(a,bytes([0xa5])*120);put(a+0x68,owner)
  chain(owner+0xb0,nodes);chain(destination,old)
  before={a:bytes(u.mem_read(a,120)) for a in nodes+old};put(0x7a3bf8,27);put(0x7a3cf4,42)
  call(0x497230,owner);verify_chain(owner+0xb0,[]);verify_chain(destination,old+nodes)
  assert get(0x7a3bf8)==27 and get(0x7a3cf4)==42
  for a in nodes+old:
   expected=bytearray(before[a]);expected[:8]=u.mem_read(a,8)
   if a in nodes:expected[0x68:0x6c]=bytes(4)
   assert bytes(u.mem_read(a,120))==expected
  cases+=1
# Expired records return to the tail of their own free list, without a draw.
expiry_cases=0
for kind in (0,1):
 for count in (1,2,8):
  for existing in (0,1,3):
   for reason in ('life','radius'):
    nodes=[base+0x2000+i*120 for i in range(count)];old=[base+0x6000+i*120 for i in range(existing)]
    active=base+0x5000;free=0x7a3b08+kind*0xfc;counter=0x7a3bf8+kind*0xfc
    for a in nodes:
     u.mem_write(a,bytes([0xa5])*120);put(a+8,0xffffffff);u.mem_write(a+0xc,struct.pack('<3f',1,2,3))
     u.mem_write(a+0x24,struct.pack('<f',0.5));u.mem_write(a+0x34,struct.pack('<3f',0.75 if reason=='life' else 10,1 if reason=='life' else 0.25,-1));u.mem_write(a+0x50,bytes([kind]))
    for a in old:u.mem_write(a,bytes([0x5a])*120)
    chain(active,nodes);chain(free,old);put(counter,count);before={a:bytes(u.mem_read(a,120)) for a in nodes+old}
    u.mem_write(stack,struct.pack('<IIf',stop,active,0.25));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x495120,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop;verify_chain(active,[]);verify_chain(free,old+nodes);assert get(counter)==0
    for a in nodes+old:
     expected=bytearray(before[a]);expected[:8]=u.mem_read(a,8)
     if a in nodes:
      expected[0x24:0x28]=struct.pack('<f',0.75);expected[0x38:0x3c]=struct.pack('<f',0.75 if reason=='life' else 0)
      expected[0x58:0x5c]=bytes(4);expected[0x6c:0x78]=expected[0xc:0x18]
     assert bytes(u.mem_read(a,120))==expected
    expiry_cases+=1
report=dict(result='PASS',pool_capacities=[500,1100],record_bytes=120,storage_bytes=192000,detach_cases=cases,expiry_cases=expiry_cases,scope='Unchanged original 494e70 initializes both fixed pools; unchanged 497230 detaches live particles in order without freeing them or changing counts. Original 495120 expiry returns records to their pool tail and decrements counts. Shared pool implementation and rendering remain open.')
(root/'artifacts/particle-pool-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
