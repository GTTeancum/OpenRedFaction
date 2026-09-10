"""Differential fixed-pool command replay: original, PC and compiled NXDK."""
import runpy,struct,random,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_pools.py')))
u=c['u'];x=c['ctx']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop'];thread=c['ctx']['thread'];put=c['put'];get=c['get']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EBX
x.mem_map(base+0x10000,0x30000);storage=base+0x10000;pool=base+0x4000;lists=base+0x5000
from unicorn import UC_HOOK_CODE
u.hook_add(UC_HOOK_CODE,lambda m,a,size,data: m.emu_stop() if a==0x495697 else None,begin=0x495697,end=0x495697)
symbols=(root/'build/xbox/main.map').read_text()
entries={n:int(re.search('_rf_particle_pool_'+n+r'\s+([0-9a-fA-F]+)',symbols)[1],16) for n in ('init','create','detach','recycle')}
def xcall(name,*args):
 x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[name],stop,count=5000000)
 assert x.reg_read(UC_X86_REG_EIP)==stop,(name,hex(x.reg_read(UC_X86_REG_EIP)))
 return struct.unpack('<i',struct.pack('<I',x.reg_read(UC_X86_REG_EAX)))[0]
nodes=[0x7a3cf8+i*120 for i in range(500)]+[0x782748+i*120 for i in range(1100)]
owners=[base+0x6000+i*0x200 for i in range(3)]
sentinels=[0x7a3b08,0x7a3c04,0x7a3b80,0x7a3c7c,0x7bd670]+[a+0xb0 for a in owners]
handles={a:i for i,a in enumerate(nodes+sentinels)};emitter_handles={0:0,**{a:i+1 for i,a in enumerate(owners)}}
for start,count in ((nodes[0],500),(nodes[500],1100)):u.mem_write(start,bytes(count*120))
c['call'](0x494e70)
for a in sentinels[4:]:c['chain'](a,[])
assert xcall('init',pool,storage,lists,8)==0
commands=bytearray();expected=bytearray();rng=random.Random(496840);snapshots=operations=created=exhausted=recycled=0

def snapshot():
 data=bytearray(u.mem_read(nodes[0],500*120)+u.mem_read(nodes[500],1100*120))
 for i in range(1600):
  off=i*120
  for j in (0,4):struct.pack_into('<I',data,off+j,handles[struct.unpack_from('<I',data,off+j)[0]])
  struct.pack_into('<I',data,off+0x68,emitter_handles[struct.unpack_from('<I',data,off+0x68)[0]])
 headers=b''.join(struct.pack('<II',handles[get(a)],handles[get(a+4)]) for a in sentinels)
 assert bytes(x.mem_read(storage,192000))+bytes(x.mem_read(lists,64))==bytes(data)+headers,('snapshot',operations)
 return bytes(data)+headers

def command(op,kind=0,emitter=0,index=0xfeedface,invalid=False):
 global snapshots,operations,created,exhausted,recycled
 seed=rng.getrandbits(32);owner=0xffffffff;room=0x1234
 spawn=struct.pack('<11f6IfI',*(rng.randint(-100,100)/16 for _ in range(11)),23,16,0xff112233,0xff445566,rng.choice((0,16,512,528)),0x1234,0.5,0)
 assert len(spawn)==76
 commands.extend(struct.pack('<7I',op,kind,owner,room,emitter,index,seed)+spawn)
 x.mem_write(base,spawn);x.mem_write(base+0x200,struct.pack('<II',seed,index));put(thread+0x14,seed);put(base+0x300,index)
 status=0;result_index=index
 if op==0:
  if invalid:status=-4
  else:
   u.mem_write(base,spawn);u.mem_write(stack,struct.pack('<8I',stop,kind,base,room,0,owner,base+0x300,owners[emitter-1] if emitter else 0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x496840,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
   address=get(base+0x300)
   if address==index:status=-3;exhausted+=1
   else:result_index=handles[address];created+=1
  actual=xcall('create',pool,kind,base,owner,room,emitter,base+0x200,base+0x204)
 elif op==1:
  if invalid:status=-4
  else:c['call'](0x497230,owners[emitter-1])
  actual=xcall('detach',pool,emitter)
 elif op==2:
  if invalid:status=-4
  else:
   u.reg_write(UC_X86_REG_ESI,nodes[index]);u.reg_write(UC_X86_REG_EBX,0);u.emu_start(0x495615,0x495697,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x495697;recycled+=1
  actual=xcall('recycle',pool,index)
 else:actual=0
 assert actual==status,(operations,op,actual,status)
 state=struct.pack('<i4I',status,result_index,get(thread+0x14),get(0x7a3bf8),get(0x7a3cf4))
 xseed,xindex=struct.unpack('<II',x.mem_read(base+0x200,8));xlive=bytes(x.mem_read(pool+12,8))
 assert struct.pack('<iII',actual,xindex,xseed)+xlive==state,(operations,op)
 expected.extend(state);operations+=1
 if op==3:expected.extend(snapshot());snapshots+=1

command(3)
for kind,count in ((0,500),(1,1100)):
 for i in range(count):command(0,kind,emitter=i%4)
 command(0,kind,emitter=1);command(0,kind)
command(3)
for emitter in (1,2,3,1):command(1,emitter=emitter)
command(3)
for i in range(0,1600,7):command(2,index=i)
command(3)
for kind in (0,1):
 while get(0x7a3bf8+kind*0xfc)<(500 if kind==0 else 1100):command(0,kind,emitter=2)
command(3)
order=list(range(1600));rng.shuffle(order)
for i in order:command(2,index=i)
command(3)
for args in (dict(op=2,index=0),dict(op=2,index=1600),dict(op=0,kind=2),dict(op=0,emitter=4),dict(op=1,emitter=0),dict(op=1,emitter=4)):command(**args,invalid=True)
command(3)
output=subprocess.check_output([str(c['ctx']['ctx']['probe']),'--particle-pool'],input=commands)
assert output==expected,('PC',len(output),len(expected))
report=dict(result='PASS',operations=operations,snapshots=snapshots,created=created,exhausted=exhausted,recycled=recycled,scope='Original allocation/detachment and recycling span versus shared PC/NXDK. Full storage/list snapshots normalize pointers to handles. Saturation, reuse, mixed owners and invalid operations; simulation/rendering excluded.')
(root/'artifacts/particle-pool-runtime-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
