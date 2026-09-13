"""Execute complete original4087a0 event arbitration with explicit service boundaries."""
import hashlib,json,random,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
B=0x30000000;u.mem_map(B,0x10000);INV=B+0x2a0;EVENT=B+0x2000;PEERS=[B,B+0x4000,B+0x6000];STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];cfg={}
def ret(value):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value&0xffffffff);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 if address==0x4b05f0:trace.append(['global',cfg['gate']]);ret(cfg['gate'])
 elif address==0x4b6800:
  assert arg(0)==77;trace.append(['event',77]);ret(EVENT if cfg['event_present'] else 0)
 elif address==0x426fc0:
  assert arg(0)==55;trace.append(['actor',55]);ret(B if cfg['actor_present'] else 0)
 elif address in (0x40a110,0x427020):
  i=PEERS.index(arg(0));value=cfg['peers'][i]['busy' if address==0x40a110 else 'dead'];trace.append(['busy' if address==0x40a110 else 'dead',i,value]);ret(value)
 elif address==0x40ac90:
  assert arg(0)==55 and arg(1)==EVENT+0x40;trace.append(['destination',55,cfg['destination']]);ret(cfg['destination'])
 elif address==0x4280b0:
  assert arg(0)==B;trace.append(['stance']);u.mem_write(EVENT+0x40,struct.pack('<3f',*cfg['changed_position']));ret(0)
 elif address==0x407e20:
  assert [arg(i) for i in range(4)]==[INV,16,0xffffffff,0xffffffff];trace.append(['action',16])
 elif address==0x407e80:
  assert [arg(i) for i in range(2)]==[INV,1];trace.append(['state',1])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x4087a0);records=[];paths=set();starts=0
for case in range(2048):
 cfg=dict(scalar_bits=rng.choice((0,0x80000000,0x3f800000,0xbf800000,0x7f800000,0xff800000,0x7fc00000,1)),gate=rng.choice((0,1,2,256,257)),event_present=True,actor_present=True,event_type=46,destination=rng.choice((0,1,2,256,257)),network=case%4,clock=12345.75,changed_position=[case/8,-3.5,19.25])
 # Regular positive entry cases retain exhaustive randomized peer filtering.
 if case%4:cfg.update(scalar_bits=0x3f800000,gate=0)
 if case%16==4:cfg['event_present']=False
 if case%16==5:cfg['actor_present']=False
 if case%16==6:cfg['event_type']=45
 cfg['peers']=[dict(handle=55 if i==0 else 55 if case%8==0 else 60+i,busy=rng.choice((0,1,2,256,257)),dead=rng.choice((0,1,2,256,257)),event=rng.choice((77,78)),action=rng.choice((3,16,17))) for i in range(3)]
 cfg['peer_count']=case%4
 seed=bytearray(rng.randbytes(0x1500));seed[0x2c:0x30]=w(55);seed[0x2a0:0x2a4]=w(B);seed[0x76c:0x770]=w(77);seed[0x8c0:0x8c4]=w(cfg['scalar_bits'])
 cfg['peers'][0]['event']=77;cfg['peers'][0]['action']=struct.unpack_from('<I',seed,0x520)[0]
 u.mem_write(B,bytes(seed));u.mem_write(EVENT,bytes(0x300));u.mem_write(EVENT+0x290,w(cfg['event_type']));u.mem_write(EVENT+0x40,struct.pack('<3f',1,2,3))
 for i,p in enumerate(PEERS):
  if i:
   u.mem_write(p,bytes(0x1500));v=cfg['peers'][i];u.mem_write(p+0x2c,w(v['handle']));u.mem_write(p+0x76c,w(v['event']));u.mem_write(p+0x520,w(v['action']))
  u.mem_write(p+0x28c,w(PEERS[i+1] if i+1<cfg['peer_count'] else 0x5cb060))
 # Linked-list pointer is input, not a mutation by the routine.
 seed=bytes(u.mem_read(B,len(seed)));peer_before=[bytes(u.mem_read(p,0x1500)) for p in PEERS[1:]]
 u.mem_write(0x5cb2ec,w(B if cfg['peer_count'] else 0x5cb060));u.mem_write(0x6fc4d8,bytes([cfg['network']&1]));u.mem_write(0x64ecb9,bytes([cfg['network']>>1]));u.mem_write(0x6460f0,struct.pack('<f',cfg['clock']))
 u.mem_write(STACK,w(STOP,INV));u.reg_write(UC_X86_REG_ESP,STACK);trace=[];u.emu_start(0x4087a0,STOP,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==STACK+4
 result=u.reg_read(UC_X86_REG_EAX)&255;assert result in (0,1)
 # Independent gate/ordered-call model; nonzero and exactly-one tests differ.
 expected_trace=[];scalar=struct.unpack('<f',w(cfg['scalar_bits']))[0];accepted=False
 if scalar>0:
  expected_trace.append(['global',cfg['gate']])
  if cfg['gate']&255!=1:
   expected_trace.append(['event',77])
   if cfg['event_present'] and cfg['event_type']==46:
    expected_trace.append(['actor',55])
    if cfg['actor_present']:
     occupied=False
     for i,v in enumerate(cfg['peers'][:cfg['peer_count']]):
      expected_trace.append(['busy',i,v['busy']])
      if not v['busy']&255 and v['handle']!=55:
       expected_trace.append(['dead',i,v['dead']])
       if v['dead']&255!=1 and v['event']==77 and v['action']==16:occupied=True;break
     if not occupied:
      expected_trace.append(['destination',55,cfg['destination']])
      if cfg['destination']&255:accepted=True;expected_trace.extend([['stance'],['action',16],['state',1]])
 assert trace==expected_trace,(case,trace,expected_trace)
 assert result==int(accepted),(case,result,accepted)
 want=bytearray(seed)
 if accepted:
  want[0x6ec:0x6f8]=struct.pack('<3f',*cfg['changed_position'])
  for off,value in ((0x520,16),(0x528,12345),(0x52c,-1),(0x530,-1),(0x554,1),(0x55c,12345)):want[off:off+4]=w(value)
  flags=struct.unpack_from('<I',seed,0x7d0)[0]
  if cfg['network']:flags&=0xf07fffff
  want[0x7d0:0x7d4]=w(flags&0xffdfffff)
 assert bytes(u.mem_read(B,len(seed)))==want,('footprint',case)
 assert [bytes(u.mem_read(p,0x1500)) for p in PEERS[1:]]==peer_before
 starts+=result;paths.add(tuple(tuple(t) for t in trace))
 offsets=(0x520,0x528,0x52c,0x530,0x554,0x55c,0x7d0,0x6ec,0x6f0,0x6f4)
 records.append(dict(case=case,input=cfg,result=result,trace=trace,initial=[struct.unpack_from('<I',seed,o)[0] for o in offsets],final=[r(B+o) for o in offsets]))
report=dict(result='PASS',cases=len(records),accepted=starts,distinct_traces=len(paths),scope='Full original4087a0, actual vector copy and action/state setters. Global/event/actor lookup, peer predicates, destination and stance services supplied. Exact entire actor and peer footprints, ordered calls, low-byte gates, scalar edge cases, and post-stance event-position read. No shared or live scheduling claim.',records=records)
(root/'artifacts/ai-arbitration-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
