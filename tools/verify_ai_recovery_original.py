"""Original408f20 recovery ordering, real predicates and timer conversions."""
import hashlib,json,random,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;u.mem_map(b,0x10000);inv=b+0x2a0;linked=b+0x3000;cls=b+0x4000;stack=b+0xe000;stop=b+0xf000;stub=stop+0x100;duration=stop+0x200
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<f',v)
r=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
u.mem_write(stub,b'\xd9\x05'+w(duration)+b'\xc3');trace=[];cfg={};timer_offsets=[]
def ret(value):
 sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,r(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i)
 if address==0x426fc0:
  assert arg(0)==55;trace.append(['actor',55]);ret(b if cfg['present'] else 0)
 elif address==0x40a0e0:
  assert arg(0)==77;trace.append(['linked',77]);ret(linked if cfg['linked'] else 0)
 elif address==0x503400:
  assert arg(0)==700;trace.append(['stop',700]);ret(0)
 elif address==0x428c90:
  assert bytes(cpu.mem_read(sp+4,20))==w(b,40,0x3f800000,0,1)
  trace.append(['start',40]);cpu.mem_write(b+0x80,w(701));cpu.mem_write(b+0xcd4,w(19));ret(0)
 elif address==0x5033e0:
  assert (arg(0),arg(1))==(701,19);trace.append(['duration',701,19]);cpu.reg_write(UC_X86_REG_EIP,stub)
 elif address==0x4fa360:
  destination=cpu.reg_read(UC_X86_REG_ECX);assert destination in (inv+0x274,inv+0x278)
  timer_offsets.append(struct.unpack('<i',w(arg(0)))[0]);trace.append(['timer',destination-inv,arg(0)])
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x408f20);records=[];accepted=0;paths=set();period=1072800000
for case in range(2048):
 cfg=dict(present=case%11!=0,action=13 if case%3==0 else 3,linked=bool(case&1),health=(-1.0,0.0,1.0,float('nan'))[(case//2)%4],blocked=bool((case//8)%2),dead=bool((case//16)%2),motion=-1 if case%7==0 else 18)
 cfg['now']=(0,1,period-1,period)[case%4] if case<128 else rng.randrange(period+1)
 cfg['deadline']=(-1,cfg['now'],(cfg['now']+100)%period,(cfg['now']-100)%period)[(case//32)%4]
 cfg['duration']=(-.0019,0,.0009,.0019,.1239,1.9999,5.25)[case%7];cfg['delay']=(-.1259,0,.0009,.9999,10.25)[case%5]
 seed=bytearray(rng.randbytes(0x1500));seed[0x2c:0x30]=w(55);seed[0x80:0x84]=w(700);seed[0x200:0x204]=w(77);seed[0x294:0x298]=w(cls);seed[0x2a0:0x2a4]=w(b)
 seed[0x520:0x524]=w(cfg['action']);seed[0x514:0x518]=w(cfg['deadline']);seed[0x7d0:0x7d4]=w(0x100 if cfg['blocked'] else 0);seed[0x810:0x814]=w(1 if cfg['dead'] else 0);seed[0xcd4:0xcd8]=w(cfg['motion'])
 u.mem_write(b,bytes(seed));u.mem_write(linked+0x34,f(cfg['health']));u.mem_write(cls+0xf78,f(cfg['delay']));u.mem_write(duration,f(cfg['duration']));u.mem_write(0x5a3ed8,w(cfg['now']))
 u.mem_write(stack,w(stop,inv));u.reg_write(UC_X86_REG_ESP,stack);trace=[];timer_offsets=[];u.emu_start(0x408f20,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 early=not cfg['present'] or (cfg['action']==13 and cfg['linked'] and cfg['health']>0)
 expected=bytearray(seed)
 if not early and cfg['action']==13:expected[0x7bc:0x7c0]=w(0)
 dl=cfg['deadline'];now=cfg['now'];pending=dl>=0 and ((dl>now and dl-now<=period//2) or (dl<=now and now-dl>period//2))
 proceeds=not early and cfg['blocked'] and not pending and not cfg['dead'] and cfg['motion']!=-1
 if proceeds:
  expected[0x80:0x84]=w(701);expected[0xcd4:0xcd8]=w(19)
  offsets=[int(struct.unpack('<f',f(v))[0]*1000) for v in (cfg['duration'],cfg['delay'])]
  assert timer_offsets==offsets
  for off,value in zip((0x514,0x518),offsets):
   deadline=now+value
   if deadline>period:deadline-=period
   if deadline<0:deadline+=period
   expected[off:off+4]=w(deadline)
  assert [t[0] for t in trace][-5:]==['stop','start','duration','timer','timer'];accepted+=1
 else:assert not timer_offsets and not any(t[0]=='stop' for t in trace)
 assert bytes(u.mem_read(b,len(seed)))==expected,('actor footprint',case)
 paths.add(tuple(t[0] for t in trace))
 # Store float bits as well, so NaN input remains an exact reproducible value.
 for name in ('health','duration','delay'):cfg[name+'_bits']=struct.unpack('<I',f(cfg.pop(name)))[0]
 records.append(dict(case=case,input=cfg,trace=trace,timers=[r(inv+0x274),r(inv+0x278)],word_7bc=r(b+0x7bc)))
report=dict(result='PASS',cases=len(records),accepted=accepted,paths=len(paths),scope='Full original408f20 with actual408ef0/408ec0/427020, __ftol and4fa360. Only actor/object lookup and playback stop/start/duration supplied. Exact actor footprint, gate/timer semantics, callback-modified model/motion rereads. Not shared C or native XEMU.',records=records)
(root/'artifacts/ai-recovery-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='records'})
