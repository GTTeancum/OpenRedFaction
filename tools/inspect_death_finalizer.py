"""Execute full original418f80 in ordinary SP with supplied resource boundaries.

Actor/list reads, classification, player detach, strings and placement vector
math execute original machine code. Damage, attachment changes, explosion,
queries, corpse construction, burn and area are controlled backend boundaries.
This does not claim those supplied effects or the live finalizer are integrated.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0,4096)
b=0x30000000;u.mem_map(b,0x100000);actor=b+0x1000;cls=b+0x3000;corpse=b+0x6000;parent=b+0x9000;child=b+0xb000
player=b+0x16000;records=b+0x18000;names=b+0x19000;region=b+0x1a000;floatret=b+0x1b000;heap=b+0x30000;stack=b+0xe0000;stop=b+0xf0000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
put=lambda a,*v:u.mem_write(a,w(*v))
string=lambda a:bytes(u.mem_read(a,128)).split(b'\0')[0]
trace=[];allocated={};cursor=heap;query_payload=b'';captured=[]
# This fixture supplies only the side effects named below. Unknown calls run.
controlled={0x573619,0x57360e,0x4892c0,0x4279d0,0x427380,0x4a3740,0x419420,0x45cd20,0x499ed0,0x4dfd40,0x416940,0x4174f0,0x42f510,0x42ed20,0x42a8e0}
def hook(cpu,address,size,data):
 global cursor
 if address not in controlled:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+4*i);ecx=cpu.reg_read(UC_X86_REG_ECX);ret=0;pop=0;float_value=None
 if address==0x573619:
  n=arg(0);assert n<1024;ret=cursor;cursor+=(n+15)&~15;allocated[ret]=n;cpu.mem_write(ret,bytes(n))
 elif address==0x57360e:
  a=arg(0);assert a in allocated;cpu.mem_write(a,b'\xdd'*allocated.pop(a))
 elif address==0x4892c0:
  trace.append(('damage',*[arg(i) for i in range(8)]));float_value=0.0
 elif address==0x4279d0:assert arg(0)==actor;trace.append(('parent_detach',))
 elif address==0x427380:
  assert ecx==actor and arg(1)==1;trace.append(('child_detach',arg(0),read(child+0x34)));pop=8
 elif address==0x4a3740:assert arg(0)==100;trace.append(('player_lookup',));ret=player if has_player else 0
 elif address==0x419420:
  assert arg(0)==actor and read(actor+0x824)==0xffffffff;trace.append(('explosion',))
 elif address==0x45cd20:
  assert arg(0)==actor+0x3c;trace.append(('region',));ret=region if in_region else 0
 elif address==0x499ed0:
  assert arg(2)==actor+0x88
  endpoints=bytes(cpu.mem_read(arg(0),12))+bytes(cpu.mem_read(arg(1),12))
  assert endpoints==f(2,4.5,6,2,2.5,6),endpoints
  result=arg(3);assert read(result+0x18)==0x3f800000 and read(result+0x30)==0xffffffff and read(result+0x38)==0, (case, hex(result), bytes(cpu.mem_read(result,68)).hex())
  cpu.mem_write(result,query_payload);trace.append(('probe',))
 elif address==0x4dfd40:assert ecx==0x1234;trace.append(('area',));float_value=area
 elif address==0x416940:
  assert arg(0)==actor and arg(2)==actor+0x3c and arg(3)==actor+0x48 and arg(4)==arg(5)==0
  trace.append(('create',string(arg(1)).decode()));captured.append(bytes(cpu.mem_read(actor+0x48,36)))
  put(actor+0x7c,read(actor+0x7c)|2|(0 if replacement else 0x400))
  ret=0 if fail_create else corpse
 elif address==0x4174f0:
  assert arg(0)==corpse;trace.append(('drop',read(actor+0x810)))
  if mutate:put(actor+0x810,read(actor+0x810)|0x800)
 elif address==0x42f510:
  assert arg(0)==burn and arg(1)==200;trace.append(('retarget',arg(0),arg(1)))
  if mutate:put(actor+0x13d8,burn+1)
 elif address==0x42ed20:
  assert arg(0)==burn and arg(1)==0;trace.append(('release',arg(0)))
 elif address==0x42a8e0:assert arg(0)==actor;trace.append(('tail_predicate',));ret=1
 if float_value is not None:
  cpu.mem_write(floatret,b'\xd9\x05'+w(floatret+16)+b'\xc3');cpu.mem_write(floatret+16,f(float_value));cpu.reg_write(UC_X86_REG_EIP,floatret);return
 cpu.reg_write(UC_X86_REG_EAX,ret);cpu.reg_write(UC_X86_REG_EIP,read(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x418f80);totals=dict(create=0,drop=0,retarget=0,release=0,aligned=0,blocked_object=0,probe=0,damage=0,explosion=0)
for case in range(1024):
 # Product coverage varies both the early gates and later placement/resource branches.
 kind=(0,1)[case&1];special=bool(case&2);has_parent=bool(case&4);has_player=bool(case&8);explosion=bool(case&16)
 skip=bool(case&32);action=(-1,0,1)[(case//3)%3];replacement=bool(case&64);in_region=bool(case&128);mode=10 if case%5==0 else 0
 hit=(case%4)!=0;resolved=bool(case&256);normal_y=(.5,.5001,1.0)[case%3];face=bool(case&512);area=(1.0,1.0001,4.0)[(case//5)%3]
 fail_create=case%7==0;burn=0x99 if case%3 else 0;drop=bool(case&1);mutate=bool(case&4)
 trace.clear();captured.clear();allocated.clear();cursor=heap
 u.mem_write(actor,bytes(0x1500));u.mem_write(cls,bytes(0x1500));u.mem_write(corpse,b'\xa5'*0x300)
 put(actor+0x24,0);put(actor+0x2c,100);put(actor+0x294,cls);put(cls+0x1b4,kind)
 initial_flags=rng.getrandbits(32)&~0x402;put(actor+0x7c,initial_flags)
 flags=(0x80 if skip else 0)|(0x4000000 if drop else 0)|0x201;put(actor+0x810,flags)
 put(actor+0x7d0,0x100000 if special else 0);put(actor+0x200,101 if has_parent else -1)
 put(actor+0x824,action);put(cls+0x6f8,42 if explosion else -1);put(actor+0x13d8,burn)
 u.mem_write(actor+0x3c,f(2,4,6));u.mem_write(actor+0x48,f(1,0,0,0,1,0,0,0,1));u.mem_write(actor+0x1b4,b'\xa5'*68)
 put(actor+0x858,records+0x200);put(records+0x204,mode)
 u.mem_write(names,b'corpse.v3d\0');put(cls+0x18,10 if replacement else -1,names if replacement else 0x62f3d0)
 put(cls+0x760,names+0x100);u.mem_write(names+0x200,b'death_test\0');put(names+0x100,10,names+0x200)
 put(actor+0xa58,0);put(actor+0xa68,-1)
 for a,h,par in ((parent,101,-1),(child,102,100)):
  u.mem_write(a,bytes(0x1500));put(a+0x24,0);put(a+0x2c,h);put(a+0x200,par);put(a+0x7d0,0x100000 if special else 0);u.mem_write(a+0x34,f(42))
 u.mem_write(0x7394cc,bytes(4096))
 for h,a in ((100,actor),(101,parent),(102,child),(200,corpse)):
  put(0x7394cc+4*h,a)
 put(corpse+0x2c,200);put(corpse+0x24,7)
 # A separate object is returned for a probe hit, with a real handle lookup.
 put(0x7394cc+4*201,region if resolved else 0);put(region+0x2c,201)
 put(actor+0x8cc,1,1,records);put(records,records+0x100);put(records+0x104,102)
 put(0x7c7634,1);put(0x7c75e4,records+0x300);put(records+0x314,102)
 u.mem_write(player,bytes(0x1300));put(player+0x14,100);u.mem_write(player+0xfb0,b'\x01\xa5\xa5\xa5')
 put(region+8,2);u.mem_write(0x64ecb9,b'\0');u.mem_write(0x6fc4d8,b'\0')
 query_payload=f(2,3,6,0,normal_y,0,0.5 if hit else 1)+w(0x111,0x222)+f(7,8,9)+w(201 if resolved else -1,0x333,0x444,0x1234 if face else 0,0x555)
 assert len(query_payload)==68
 original=bytes(u.mem_read(actor,0x1500));u.mem_write(0,w(0));u.mem_write(stack,w(stop,actor));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x418f80,stop,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and not allocated,(case,hex(u.reg_read(UC_X86_REG_EIP)),allocated)
 names_called=[t[0] for t in trace];eligible=not skip and ((-1 if explosion else action)!=-1 or replacement)
 probed=eligible and not in_region and mode!=10;created=eligible and not in_region and (mode==10 or not hit or not resolved)
 success=created and not fail_create;aligned=probed and hit and not resolved and normal_y>.5 and face and area>1
 assert ('probe' in names_called)==probed and ('create' in names_called)==created,(case,trace)
 assert ('area' in names_called)==(probed and hit and not resolved and normal_y>.5 and face),(case,trace)
 assert ('drop' in names_called)==(success and drop)
 assert ('retarget' in names_called)==bool(success and burn)
 assert ('release' in names_called)==bool(not success and burn)
 assert read(actor+0x7c)==(initial_flags|2|(0x400 if created and not replacement else 0))
 assert read(actor+0x824)==((-1 if explosion else action)&0xffffffff)
 expected_flags=flags
 if success and drop:expected_flags=(expected_flags|(0x800 if mutate else 0))&~0x200
 assert read(actor+0x810)==expected_flags
 assert read(actor+0x13d8)==(burn+(1 if mutate else 0) if success and burn else 0)
 if success and burn:assert read(corpse+0x2d0)==burn+(1 if mutate else 0)
 else:assert read(corpse+0x2d0)==0xa5a5a5a5
 assert read(player+0x14)==(0xffffffff if has_player else 100)
 assert bytes(u.mem_read(player+0xfb0,4))==(b'\0' if has_player else b'\x01')+b'\xa5'*3
 assert ('parent_detach' in names_called)==has_parent
 damages=[t for t in trace if t[0]=='damage'];assert len(damages)==kind+int(special and has_parent),(case,damages)
 if kind:assert damages[0]==('damage',102,0x461c4000,0xffffffff,0xffffffff,3,0,0xffffffff,0)
 if special and has_parent:assert damages[-1]==('damage',101,0x447a0000,0xffffffff,0xffffffff,0xffffffff,0,0xffffffff,0)
 assert ('child_detach',102,0 if special else 0x42280000) in trace
 assert bytes(u.mem_read(actor+0x1b4,68))==(query_payload if aligned else b'\xa5'*68)
 if aligned:assert bytes(u.mem_read(actor+0x48,36))==f(1,0,0,0,normal_y,0,0,0,normal_y)
 if not aligned:assert bytes(u.mem_read(actor+0x48,36))==original[0x48:0x6c]
 if created:
  death_name='death_test' if not explosion and action==0 else ''
  assert ('create',death_name) in trace
 # Enforce whole source preservation outside documented stores and copied hit/basis.
 expected=bytearray(original)
 for off,n in ((0x7c,4),(0x810,4),(0x824,4),(0x13d8,4),(0x48,36),(0x1b4,68)):expected[off:off+n]=u.mem_read(actor+off,n)
 assert bytes(u.mem_read(actor,len(expected)))==expected
 for key in ('create','drop','retarget','release','probe','explosion'):totals[key]+=names_called.count(key)
 totals['damage']+=len(damages);totals['aligned']+=aligned;totals['blocked_object']+=probed and hit and resolved
 if success and burn:assert names_called.index('retarget')<names_called.index('tail_predicate') and names_called[-1]=='tail_predicate'
 if not success and burn:assert names_called[-2:]==['tail_predicate','release']
 if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=1024,totals=totals,original_sha256=sha,scope=__doc__)
(root/'artifacts/death-finalizer-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
