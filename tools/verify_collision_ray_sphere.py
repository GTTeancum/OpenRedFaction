"""Full original508e40 and real vector callees versus PC/NXDK, no hooks."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_ray_sphere\s+([0-9a-fA-F]+)',mapping)[1],16)
def run(m,address,args,payload):
 m.mem_write(b,payload);m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=20000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
 assert bytes(m.mem_read(b,44))==payload[:44]
 return w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(b+44,16))
rng=random.Random(0x508e40);commands=[];answers=[];counts=[0,0];changed_miss=0
for case in range(8192):
 origin=[rng.randrange(-32,33)/4 for _ in range(3)];direction=[rng.randrange(-16,17) for _ in range(3)]
 magnitude=math.sqrt(sum(v*v for v in direction));direction=[v/magnitude for v in direction] if magnitude else [0,0,1]
 length=rng.choice((0,.125,.5,1,2,8,32));radius=rng.choice((0,.125,.5,1,2,4,8));center=[rng.randrange(-48,49)/4 for _ in range(3)]
 if case<512:
  origin=[0,0,0];direction=[0,0,1];length=(0,.5,1,2,4,8,16,32)[case%8];radius=(0,.5,1,2)[case//8%4]
  center=[(0,.5,1,2)[case//32%4],0,(-1,0,.5,2)[case//128]]
 if 512<=case<1024:
  origin=[0,0,0];direction=[0,0,1];length=2;radius=1
  center=[struct.unpack('<f',w(0x3f800000+(case%3)-1))[0],0,2+(case//3%3-1)*2**-22]
 payload=f(origin+direction+[length]+center+[radius,91,92,93,94]);ints=struct.unpack('<15I',payload);commands.append(payload)
 expected=run(u,0x508e40,[b+44,b+56,b,ints[6],b+28,ints[10]],payload)
 actual=run(x,entry,[b,ints[6],b+28,ints[10],b+44,b+56],payload)
 assert actual==expected,(case,payload.hex(),expected.hex(),actual.hex())
 result=struct.unpack('<I',expected[:4])[0];counts[result]+=1
 changed_miss+=not result and expected[16:20]!=payload[56:60]
 answers.append(expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--ray-sphere'],input=b''.join(commands));expected=b''.join(answers)
if actual!=expected:
 for i in range(len(answers)):
  assert actual[i*20:i*20+20]==answers[i],('PC',i,commands[i].hex(),answers[i].hex(),actual[i*20:i*20+20].hex())
assert all(counts) and changed_miss
report=dict(result='PASS',cases=len(commands),misses=counts[0],hits=counts[1],misses_updating_distance=changed_miss,original_sha256=sha,x87_control='0x027f',scope='Full original508e40 with real vector helpers, no hooks. Exact return, point, fraction and input preservation versus PC/NXDK. Zero length, forward gate, initial overlap, tangency and adjacent float boundaries. Finite inputs; actor response49ab00 and live scheduling excluded.')
(root/'artifacts/collision-ray-sphere.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
