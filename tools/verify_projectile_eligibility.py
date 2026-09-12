"""Compare48c7f0 geometry/owner gates to PC/NXDK, resolving only owner lookup."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;target=b+0x2000;owner=b+0x4000;state=b+0x8000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_projectile_eligible\s+([0-9a-fA-F]+)',mapping)[1],16)
present=lookups=0;planes_seen=[]
def hook(m,address,size,data):
 global lookups
 if address==0x4163a0:planes_seen.append(m.reg_read(UC_X86_REG_ECX))
 if address!=0x426fc0:return
 lookups+=1;sp=m.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',m.mem_read(sp,8));assert arg==1234
 m.reg_write(UC_X86_REG_EAX,owner if present else 0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=10000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
 return m.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x48c7f0);commands=[];answers=[];counts=[0,0];planes_hist=[0]*5;lookup_total=0
for case in range(8192):
 geometry=[rng.randrange(-256,257)/8 for _ in range(12)]+[rng.choice((0,.25,.5,1,2,8)) for _ in range(2)]
 flags=rng.choice((0,0x20));kind=rng.choice((0,0,2,4));tf=rng.choice((0,0,1));handle=17
 present=rng.randrange(2);of=rng.choice((0,0,1));ot=rng.choice((17,18));mode=rng.choice((0,1,2,256,257,0xffffffff))
 planes=[rng.randrange(-32,33)/8 for _ in range(16)]
 if case<256:
  geometry=[0,0,0,0,0,1,0,0,1,0,0,0,.5,.5];flags=0x20;present=0;mode=1
  planes=[0,0,0,0]*4;planes[(case%4)*4+3]=(-1,-1.0000001192092896,-.9999999403953552,0)[(case//4)%4]
 if 256<=case<320:
  geometry=[0,0,0,1,1,-1,16777216,1,16777216,0,0,0,.5,.5];flags=0;present=0;mode=case%2
 facts=[flags,kind,tf,handle,present,of,ot,mode];payload=f(geometry)+w(*facts)+f(planes);assert len(payload)==152
 u.mem_write(b+0x3c,f(geometry[:3]));u.mem_write(b+0x60,f(geometry[3:6]));u.mem_write(target+0x3c,f(geometry[6:9]));u.mem_write(target+0xe4,f(geometry[9:12]));u.mem_write(b+0x180,f(geometry[12:13]));u.mem_write(target+0x180,f(geometry[13:14]))
 u.mem_write(b+0x30,w(1234));u.mem_write(b+0x294,w(b+0x6000));u.mem_write(b+0x6268,w(flags));u.mem_write(target+0x24,w(kind));u.mem_write(target+0x1f8,w(tf));u.mem_write(target+0x2c,w(handle));u.mem_write(owner+0x1f8,w(of));u.mem_write(owner+0x560,w(ot));u.mem_write(0x75db38,f(planes))
 before=bytes(u.mem_read(b,0x7000));lookups=0;planes_seen=[];result=run(u,0x48c7f0,[b,target,mode]);assert bytes(u.mem_read(b,0x7000))==before
 assert lookups in (0,1) and planes_seen==[0x75db38+16*i for i in range(len(planes_seen))]
 if not mode&255:assert lookups==0 and not planes_seen
 lookup_total+=lookups;planes_hist[len(planes_seen)]+=1;counts[result]+=1
 x.mem_write(state,payload);actual=run(x,entry,[state]);assert actual==result and bytes(x.mem_read(state,152))==payload,(case,geometry,facts,planes,result,actual)
 commands.append(payload);answers.append(w(result))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--projectile-eligible'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',cases=8192,rejected=counts[0],accepted=counts[1],owner_lookups=lookup_total,plane_count_histogram=planes_hist,x87_control='0x027f',original_sha256=sha,scope='Full original48c7f0 and real subtract/dot/plane/kind helpers; only426fc0 owner resolution supplied. Exact PC/NXDK predicate, finite geometry, boundary planes and cancellation; no input mutation. New-pair low-byte gates versus existing-pair mode0. Global plane production and live projectile ownership excluded.')
(root/'artifacts/projectile-eligibility.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
