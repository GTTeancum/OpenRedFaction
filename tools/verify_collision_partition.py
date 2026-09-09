"""Complete original segment/AABB function, including failed-attempt writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_EBP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

selected=[0]
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:selected.__setitem__(0,uc.reg_read(UC_X86_REG_EDI)),begin=0x4f90d7,end=0x4f90d7)
for address in [0x4f9271,0x4f9329]:u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=address,end=address)
rng=random.Random(0x4f9050);cases=[];expected=[];split=0
for n in range(2000):
 lo=[-rng.choice([1,2,4,8]) for _ in range(3)];hi=[rng.choice([1,2,4,8]) for _ in range(3)]
 boxes=[]
 for i in range(8):
  a=[];b=[]
  for j in range(3):
   pair=sorted(rng.choices([lo[j],(lo[j]+hi[j])/2,hi[j]],k=2));a.append(pair[0]);b.append(pair[1])
  boxes.append(a+b)
 wire=struct.pack('<54f',*lo,*hi,*[v for box in boxes for v in box]);cases.append(wire)
 node=base;faces=base+4096
 u.mem_write(node,wire[:24]+struct.pack('<II',faces,8))
 for i in range(8):
  u.mem_write(faces+i*128+0x10,wire[24+i*24:48+i*24]);u.mem_write(faces+i*128+0x58,struct.pack('<I',faces+(i+1)*128 if i<7 else 0))
 u.mem_write(stack,struct.pack('<II',stop,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,node);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4f9050,stop,count=100000);endpoint=u.reg_read(UC_X86_REG_EIP);assert endpoint in (0x4f9271,0x4f9329)
 split+=endpoint==0x4f9271;original_esp=u.reg_read(UC_X86_REG_ESP);upper=u.reg_read(UC_X86_REG_EBP);lower=struct.unpack('<I',u.mem_read(original_esp+0x54,4))[0]
 # Copy the exact half-boxes before reusing stack for predicate calls.
 halves=base+8192
 u.mem_write(halves,bytes(u.mem_read(original_esp+0x20,12))+bytes(u.mem_read(original_esp+0x14,12))+bytes(u.mem_read(original_esp+0x38,12))+bytes(u.mem_read(original_esp+0x2c,12)))
 labels=[];counts=[0,0,0]
 for i in range(8):
  label=0
  for child in range(2):
   u.mem_write(stack,struct.pack('<4I',stop,faces+i*128,halves+child*24,halves+child*24+12));u.reg_write(UC_X86_REG_ESP,stack)
   u.emu_start(0x4f8f90,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
   if u.reg_read(UC_X86_REG_EAX)&255:label=child+1;break
  labels.append(label);counts[label]+=1
 assert counts[1:]==[upper,lower]
 expected.append(struct.pack('<i4I8B',0,selected[0],*counts,*labels))
original_count=len(cases)
for offset,value in [(0,math.nan),(24,-10000),(0,10000)]:
 wire=bytearray(cases[0]);struct.pack_into('<f',wire,offset,value);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*24)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--partition'],input=b''.join(cases))
assert len(actual)==28*len(cases)
for n,want in enumerate(expected):assert actual[n*28:(n+1)*28]==want,(n,actual[n*28:(n+1)*28].hex(),want.hex())
xbox_path=root/'build/xbox/main.exe';xp=pefile.PE(str(xbox_path));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,65536)
match=re.search(r'_rf_collision_partition\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text());assert match
entry=int(match.group(1),16)
for n,(wire,want) in enumerate(zip(cases,expected)):
 node=base;faces=base+4096;out=base+12288
 x.mem_write(node,wire[:24]+bytes(16));x.mem_write(out,bytes([0xa5])*28)
 for i in range(8):x.mem_write(faces+i*72+16,wire[24+i*24:48+i*24])
 x.mem_write(stack,struct.pack('<7I',stop,node,faces,8,out+20,out+4,out+8));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+4,24));assert got==want,('NXDK',n,got.hex(),want.hex())
report=dict(result='PASS',original_cases=original_count,port_guards=3,nxdk_sha256=hashlib.sha256(xbox_path.read_bytes()).hexdigest(),cases=len(cases),splittable=split,scope='Original 4f9050 through allocation/leaf boundary; unchanged 4f8f90 used for all face labels, exact axis and group counts. Includes boundary ties and parent-spanning faces. No recursive tree allocation.')
(root/'artifacts/collision-partition-verification.json').write_text(json.dumps(report,indent=2));print(report)
