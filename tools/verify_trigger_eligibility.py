"""Original trigger eligibility with supplied actor predicates versus PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
facts=[]
def hook(m,address,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8))
 if address==0x4895d0:value=facts[2]
 elif address==0x48aaf0:value=facts[6] if arg==base+0x3000 else facts[3]
 elif address==0x426fc0:value=base+0x2000 if facts[4] else 0
 elif address==0x429990:value=facts[5]
 elif address==0x4290d0:value=facts[7]
 elif address==0x410c70:value=base+0x4000 if facts[8] else 0
 else:value=base+0x3000
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for a in (0x4895d0,0x48aaf0,0x426fc0,0x429990,0x4290d0,0x410c70,0x40a0e0):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
entry=int(re.search(r'_rf_trigger_eligible\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(406);commands=bytearray();results=bytearray();accepted=0
for case in range(4096):
 flags=(0,1,2,128,3,129,130,131,8,16,64)[case%11]
 count=rng.choice([-2,0,1,4]);limit=rng.choice([-2,-1,0,2,5]);now=(0,100,1072800000)[case%3]
 deadline=rng.choice([-1,0,now,max(0,now-1),min(1072800000,now+1)])
 filter_=(case//11)%6;attached=rng.choice([-1,0,123]);n=case%9
 handles=[rng.randrange(4) for i in range(8)];facts=[rng.randrange(4),rng.choice([0,1,2,3])]+[rng.choice([0,1,2,255,256,257]) for i in range(7)]
 facts[4]=case%2;facts[8]=(case//2)%2;input_=rng.choice([0,1,2,255,256,257])
 command=w(flags,count,limit,deadline,filter_,attached,n,*facts,now,input_,*handles);commands.extend(command)
 original=bytearray(0x400);struct.pack_into('<i',original,0x298,deadline);struct.pack_into('<ii',original,0x2a0,count,limit)
 struct.pack_into('<I',original,0x2b0,flags);struct.pack_into('<I',original,0x2c4,filter_);struct.pack_into('<i',original,0x304,attached);original[0x2d4:0x2e0]=w(n,8,base+0x800)
 actor=bytearray(0x100);actor[0x24:0x28]=w(facts[1]);actor[0x2c:0x34]=w(facts[0],88)
 u.mem_write(base,bytes(original));u.mem_write(base+0x1000,bytes(actor));u.mem_write(base+0x800,w(*handles));u.mem_write(0x5a3ed8,w(now))
 u.mem_write(stack,w(stop,base,base+0x1000,input_));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c06d0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX)&255;assert result in (0,1);accepted+=result;results.extend(w(0,result))
 assert bytes(u.mem_read(base,0x400))==original and bytes(u.mem_read(base+0x1000,0x100))==actor
 x.mem_write(base,command[:28]+w(base+0x800));x.mem_write(base+0x100,command[28:64]);x.mem_write(base+0x800,w(*handles));x.mem_write(base+0x400,w(0xa5a5a5a5))
 x.mem_write(stack,w(stop,base,base+0x100,now,input_,base+0x400));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,4))==w(0,result),case
actual=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-eligible'],input=commands)
assert actual==results
report=dict(result='PASS',cases=4096,accepted=accepted,scope='Full original 4c06d0 with timer and allowed-handle array helpers unchanged. Seven actor/registry predicates supplied; stable input snapshots. Exact decisions on PC/NXDK, untouched original trigger/actor storage. Flags, signed counts, cooldown endpoints, filters, missing entities, handle lists, low-byte inputs. Live actor resolution, contact geometry and firing excluded.')
(root/'artifacts/trigger-eligibility-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
