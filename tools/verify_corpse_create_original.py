"""Execute full original416940 with supplied allocation/resource backends."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
u.mem_map(0,4096);b=0x30000000;u.mem_map(b,65536)
actor=b;cls=b+0x2000;corpse=b+0x4000;source_spheres=b+0x5000;cloned_spheres=b+0x6000;name=b+0x7000;mesh=b+0x7100;emitter=b+0x8000
position=b+0x9000;orientation=b+0x9020;stack=b+0xe000;stop=b+0xf000
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def put(a,v):u.mem_write(a,w(v))
def string(a):return bytes(u.mem_read(a,128)).split(b'\0',1)[0]
trace=[];descriptors=[];replacement=False;fail=False;model_fail=False;emit_found=False;motion_indices=[]
# Resource effects only. String comparison, predicates, sphere copies, vector
# arithmetic, timers, retention and all constructor field writes stay original.
hooks={0x40eeb0:(1,4),0x57360e:(1,0),0x486da0:(6,0),0x4cb520:(2,0),0x502880:(3,0),0x428fe0:(2,0),0x4164c0:(1,0),0x503390:(3,0),0x4ffa80:(1,4),0x497ca0:(5,0),0x48c9a0:(1,0)}
def hook(machine,address,size,unused):
 if address==0x42dc00:
  trace.append((address,(read(machine.reg_read(UC_X86_REG_ESP)+4),)));return
 if address not in hooks:return
 argc,pop=hooks[address];esp=machine.reg_read(UC_X86_REG_ESP);ecx=machine.reg_read(UC_X86_REG_ECX);args=tuple(read(esp+4+j*4) for j in range(argc));trace.append((address,args))
 result=0
 if address==0x40eeb0:
  assert args==(16,);put(ecx+4,16);put(ecx+8,cloned_spheres)
 elif address==0x486da0:
  assert args[:3]==(7,0xffffffff,0x12340007) and args[4:]==(0,0)
  descriptors.append(bytes(machine.mem_read(args[3],0x98)));result=0 if fail else corpse
 elif address==0x4cb520:
  assert args==(actor,corpse+0x2dc);machine.mem_write(args[1],w(0x12345678,0x23456789))
 elif address==0x502880:
  assert string(args[0])==b'corpse.v3c' and args[1:]==(1,0xffffffff);result=0 if model_fail else 0x12340080
 elif address==0x428fe0:
  assert args[0]==actor
  key=string(args[1]);j=1 if key==b'corpse_drop' else 2 if key==b'corpse_carry' else 0;result=motion_indices[j]
 elif address==0x4164c0:put(corpse+0x180,0x40600000)
 elif address==0x497ca0:
  assert args==(0x12340008,0x76543210,0,corpse+0x3c,1);result=emitter if emit_found else 0
 elif address==0x57360e:assert args==(cloned_spheres,)
 machine.reg_write(UC_X86_REG_EAX,result);machine.reg_write(UC_X86_REG_ESP,esp+4+pop);machine.reg_write(UC_X86_REG_EIP,read(esp))
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x416940);successes=0;failures=0;transfers=0;nulls=0;failed_models=0;examples=[]
for case in range(1024):
 u.mem_write(b,bytes(0xa000));trace.clear();descriptors.clear()
 null=case%31==0;replacement=bool(case&1);fail=case%7==0;model_fail=case%11==0;emit_found=bool(case&2)
 flags724=rng.choice([0,0x20000,0x80000,0x200000,0x2a0000]);flags728=rng.choice([0,0x20]);flags814=rng.randrange(16)
 flags810=rng.getrandbits(24);flags7c=rng.getrandbits(16);class_index=case%3;num_spheres=case%5
 motion_indices=[rng.choice([-1,0]),rng.choice([-1,1]),rng.choice([-1,2])];motion_name=(b'death_forward',b'death_front',b'death_back',b'death_side')[case%4]
 keep=case%3;seek=(case//3)%3;kind=2 if case%5 else 1;source_model=0 if case%13==0 else 0x23450080;extra_model=0 if case%5==0 else 0x34560080
 emitter_kind=(-1,0,1)[case%3];life=(case%40)/4;now=1000
 u.mem_write(name,motion_name+b'\0');u.mem_write(mesh,b'corpse.v3c\0');u.mem_write(position,struct.pack('<3f',1,2,3));u.mem_write(orientation,struct.pack('<9f',1,0,0,0,1,0,0,0,1))
 for offset,value in [(0x20,99),(0x2c,0x12340007),(0x7c,flags7c),(0x80,source_model),(0x8c,77),(0x98,88),(0x180,0x40200000),(0x1fc,123),(0x26c,11),(0x294,cls),(0x298,class_index),(0x810,flags810),(0x814,flags814),(0x8e4,7),(0xa44,9),(0x1410,extra_model),(0x1474,55)]:put(actor+offset,value)
 for j in range(3):put(actor+0xa54+j*16,111+j)
 u.mem_write(actor+0x184,w(num_spheres,num_spheres,source_spheres))
 raw=bytes(rng.randrange(256) for _ in range(24*num_spheres));u.mem_write(source_spheres,raw)
 u.mem_write(cls+0x18,w(10 if replacement else 0,mesh if replacement else 0))
 for offset,value in [(0x28,emitter_kind),(0x44,0x42c80000),(0x94,kind),(0x724,flags724),(0x728,flags728)]:put(cls+offset,value)
 u.mem_write(cls+0x2c,struct.pack('<f',life));put(0x5cd8e4+class_index*0x1514,0x40a00000)
 put(0x6460f0,0x447a0000);put(0x5a3ed8,now);put(0x7b2770,0x76543210);put(0x7b2774,0x76543210)
 # Allocation backend supplies an initialized base object, not constructor tail fields.
 u.mem_write(corpse,bytes([0xa5])*0x318)
 for offset,value in [(0,0),(0x2c,0x12340008),(0x7c,0),(0x80,0),(0x180,0x40200000),(0x1a8,0x20 if case%2 else 0),(0x268,0)]:put(corpse+offset,value)
 u.mem_write(corpse+0x3c,bytes(u.mem_read(position,12)))
 put(0x5caed0,0);put(0x5cae44,0x5cabb8);put(0x5cae48,0x5cabb8)
 if 'prepare_list' in globals():prepare_list(globals())
 prior_count=read(0x5caed0);prior_head=read(0x5cae44);prior_tail=read(0x5cae48)
 before_actor=bytes(u.mem_read(actor,0x1494));before_corpse=bytes(u.mem_read(corpse,0x318))
 u.mem_write(stack,w(stop,0 if null else actor,name,position,orientation,keep,seek));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x416940,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX)
 if null:
  assert result==0 and not trace and bytes(u.mem_read(actor,0x1494))==before_actor;nulls+=1
  if 'observe_case' in globals():observe_case(globals())
  continue
 assert read(actor+0x7c)==(flags7c|2|(0 if replacement else 0x400))
 expected_actor=bytearray(before_actor);expected_actor[0x7c:0x80]=w(flags7c|2|(0 if replacement else 0x400))
 assert len(descriptors)==1;desc=descriptors[0]
 assert struct.unpack_from('<I',desc,12)[0]==77 and struct.unpack_from('<I',desc,20)[0]==88
 assert desc[60:72]==bytes(u.mem_read(position,12)) and desc[72:108]==bytes(u.mem_read(orientation,36))
 assert struct.unpack_from('<I',desc,132)[0]==0x40200000 and struct.unpack_from('<I',desc,148)[0]==(0x73 if flags724&0x80000 else 0x33)
 assert struct.unpack_from('<I',desc,136)[0]==num_spheres and bytes(u.mem_read(cloned_spheres,len(raw)))==raw
 if fail:
  assert result==0 and read(0x5caed0)==prior_count and bytes(u.mem_read(corpse,0x318))==before_corpse
  assert bytes(u.mem_read(actor,0x1494))==bytes(expected_actor);failures+=1
  if 'observe_case' in globals():observe_case(globals())
  continue
 successes+=1;assert result==corpse
 expected_flags=(0x80 if flags724&0x20000 else 0)|(0x400 if flags728&0x20 else 0)|(2 if flags814&8 else 0)|(0x40 if keep==1 else 0)
 transition=source_model!=0 and kind==2 and motion_indices[0]!=-1 and not flags724&0x200000
 if transition and seek==1:expected_flags|=8
 if flags724&0x200000:expected_flags|=4
 assert read(corpse+0x29c)&~1==expected_flags
 if not prior_count:assert read(corpse+0x29c)==expected_flags
 assert read(corpse+0x80)==((0 if model_fail else 0x12340080) if replacement else source_model)
 assert read(corpse+0x2b8)==(111 if transition else 0xffffffff)
 assert read(corpse+0x78)==(0x40600000 if transition else 0x40200000)
 assert read(corpse+0x2bc)==(112 if motion_indices[1]>=0 else 0xffffffff) and read(corpse+0x2c0)==(113 if motion_indices[2]>=0 else 0xffffffff)
 assert read(corpse+0x2c4)==(0 if case%4<2 else 1 if case%4==2 else 2)
 transferred=bool(flags810&0x200000 and extra_model);transfers+=transferred
 if transferred:expected_actor[0x1410:0x1414]=w(0)
 assert bytes(u.mem_read(actor,0x1494))==bytes(expected_actor)
 failed_models+=bool(replacement and model_fail)
 assert read(corpse+0x2c8)==(extra_model if transferred else 0) and read(actor+0x1410)==(0 if transferred else extra_model)
 assert read(corpse+0x268)==(emitter if emitter_kind>=0 and emit_found else 0)
 assert read(corpse+0x2ac)==(now+int(life*1000+0.5) if emitter_kind>0 else 0xffffffff)
 for offset,value in [(0x20,99),(0x26c,11),(0x1fc,123),(0x294,0x447a0000),(0x2a0,class_index),(0x34,0x42c80000),(0x2b0,0x40a00000),(0x2b4,7),(0x2d0,0),(0x2d4,0xffffffff),(0x2d8,55)]:assert read(corpse+offset)==value,(case,hex(offset),read(corpse+offset),value)
 assert bytes(u.mem_read(corpse+0x144,24))==bytes(24)
 assert read(0x5caed0)==prior_count+1 and read(0x5cae44)==(prior_head if prior_count else corpse) and read(0x5cae48)==corpse
 assert read(corpse+0x28c)==0x5cabb8 and read(corpse+0x290)==prior_tail
 assert read(corpse+0x2cc)==0xa5a5a5a5,'416940 preserves incoming sound id'
 if len(examples)<8:examples.append(dict(case=case,replacement=replacement,flags_29c=expected_flags,model=read(corpse+0x80),calls=[hex(a) for a,args in trace],writes=[hex(j) for j in range(0,0x318,4) if bytes(u.mem_read(corpse+j,4))!=before_corpse[j:j+4]]))
 if 'observe_case' in globals():observe_case(globals())
report=dict(result='PASS',cases=1024,successes=successes,allocation_failures=failures,null_sources=nulls,extra_model_transfers=transfers,null_replacement_models=failed_models,original_sha256=digest,scope='Full original416940. Supplied allocation/model/pose/emitter/string-assignment/list-reserve/collision callbacks; real predicates, string comparisons, sphere copies, timers, constructor writes and single-corpse list insertion. No shared constructor, live resources or native XEMU invocation.',examples=examples)
if globals().get('write_report',True):
 (root/'artifacts/corpse-create-original.json').write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='examples'})
