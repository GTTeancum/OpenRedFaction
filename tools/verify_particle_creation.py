"""Execute original particle allocation 496840; verify records and pool ownership.
Original pool recovery plus PC/NXDK record initialization comparison.
"""
import runpy,struct,itertools,json,re,subprocess,random
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];thread=ctx['thread'];root=ctx['root']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX

def put(a,v):u.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def packf(v):return struct.pack('<f',v)
gravity=struct.unpack('<f',u.mem_read(0x589858,4))[0]
node=base+0x2000;params=base+0x3000;owner=base+0x4000;output=base+0x5000
x=ctx['x'];entry=int(re.search(r'_rf_particle_initialize\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();results=bytearray();fixture_rng=random.Random(496840)
cases=success=0
for kind,attached,room,flags,empty,seed in itertools.product((0,1),(False,True),(0,0x12345678),(0,0x10,0x200,0x210,0xffffffff),(False,True),(0,1,0x12345678,0xffffffff)):
 free=0x7a3b08+kind*0xfc;active=0x7a3b80+kind*0xfc;count=0x7a3bf8+kind*0xfc
 u.mem_write(node,bytes([0xa5])*0x7c);u.mem_write(params,bytes(0x4c));u.mem_write(owner,bytes(0x158))
 for sentinel in (free,active,owner+0xb0):put(sentinel,sentinel);put(sentinel+4,sentinel)
 if not empty:put(free,node);put(free+4,node);put(node,free);put(node+4,free)
 put(count,7);put(output,0xfeedface);put(thread+0x14,seed)
 values=tuple(fixture_rng.randint(-10000,10000)/256 for _ in range(11))
 u.mem_write(params,struct.pack('<11f',*values));put(params+0x2c,23);put(params+0x30,0xabcd0010);put(params+0x34,0x44332211);put(params+0x38,0x88776655);put(params+0x3c,flags);put(params+0x40,0x1234);put(params+0x44,0x3f000000);put(params+0x48,0x11223344)
 before=bytes(u.mem_read(node,0x7c));u.mem_write(stack,struct.pack('<8I',stop,kind,params,room,0xdeadbeef,0x99887766,output,owner if attached else 0));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x496840,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 if empty:
  assert bytes(u.mem_read(node,0x7c))==before and get(output)==0xfeedface and get(count)==7 and get(thread+0x14)==seed
  assert get(active)==active and get(owner+0xb0)==owner+0xb0
 else:
  success+=1;dest=owner+0xb0 if attached else active
  assert get(free)==free and get(free+4)==free and get(dest)==node and get(dest+4)==node
  expected=bytearray(before)
  def word(off,val):expected[off:off+4]=struct.pack('<I',val)
  word(0,dest);word(4,dest);word(8,0x99887766)
  expected[0xc:0x18]=struct.pack('<3f',*values[:3]);expected[0x6c:0x78]=expected[0xc:0x18];expected[0x18:0x24]=struct.pack('<3f',*values[3:6]);word(0x24,0)
  for target,source in ((0x38,0x18),(0x3c,0x1c),(0x40,0x20),(0x34,0x28),(0x48,0x2c),(0x28,0x34),(0x2c,0x38),(0x30,0x34),(0x5c,0x44),(0x60,0x48)):expected[target:target+4]=u.mem_read(params+source,4)
  expected[0x44:0x48]=packf(values[9]*gravity);expected[0x4c:0x4e]=u.mem_read(params+0x30,2);expected[0x4e:0x50]=u.mem_read(params+0x40,2);expected[0x50]=kind
  nextseed=(seed*214013+2531011)&0xffffffff if flags&0x200 else seed
  angle=struct.unpack('<f',packf(6.283185307179586))[0]*((nextseed>>16)&32767)/32768 if flags&0x200 else 0
  expected[0x54:0x58]=packf(angle);word(0x58,(flags|1) if room else (flags|1)&~0x10);word(0x64,room);word(0x68,owner if attached else 0)
  actual=bytes(u.mem_read(node,0x7c));assert actual==expected,[(hex(i),a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
  assert get(output)==node and get(count)==8 and get(thread+0x14)==nextseed
  initial=struct.pack('<II',dest,dest)+before[8:120]
  command=bytes(u.mem_read(params,76))+struct.pack('<5I',kind,0x99887766,room,owner if attached else 0,seed)+initial
  commands.extend(command);result=bytes(4)+struct.pack('<I',nextseed)+actual[:120];results.extend(result)
  x.mem_write(base,command);x.mem_write(stack,struct.pack('<8I',stop,base,kind,0x99887766,room,owner if attached else 0,base+92,base+96));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
  assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
  assert bytes(4)+bytes(x.mem_read(base+92,124))==result,('NXDK',cases)

 cases+=1
# Rejected pool indices preserve caller-owned records and RNG on both targets.
for invalid_pool in (2,256,0xffffffff):
 command=bytearray(command);command[76:80]=struct.pack('<I',invalid_pool)
 commands.extend(command);result=struct.pack('<i',-4)+bytes(command[92:]);results.extend(result)
 x.mem_write(base,bytes(command));x.mem_write(stack,struct.pack('<8I',stop,base,invalid_pool,0x99887766,room,owner if attached else 0,base+92,base+96));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc
 assert struct.pack('<i',-4)+bytes(x.mem_read(base+92,124))==result
assert subprocess.check_output([str(ctx['probe']),'--particle-initialize'],input=commands)==results
report=dict(result='PASS',cases=cases,created=success,gravity_constant=gravity,original_sha256=ctx['sha'],scope='Original 496840 and callees execute; only CRT thread pointer supplied. Single-node and empty pools, emitter/global ownership, record fields, random orientation and room collision gating. PC/NXDK initialization matches all successful original records and RNG state. Pool integration and rendering excluded.')
(root/'artifacts/particle-creation-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
