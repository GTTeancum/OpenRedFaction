"""Original ambient construction/list lookup versus shared runtime ownership."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;stop=base+0xf000
u.mem_map(base,65536);u.mem_map(0,4096)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
allocations=[];registrations=[];sample=0;parameters=b''
def hook(m,a,size,context):
 if a not in (0x5054b0,0x573619,0x4ff3b0,0x4ffa80):return
 sp=m.reg_read(UC_X86_REG_ESP);pop=0;value=m.reg_read(UC_X86_REG_ECX)
 if a==0x5054b0:
  assert bytes(m.mem_read(sp+4,16))==w(base+0x500)+parameters
  registrations.append(sample);value=sample&0xffffffff
 elif a==0x573619:
  assert read(sp+4)==60;value=base+0x1000+len(allocations)*0x100
  allocations.append(value);m.mem_write(value,b'\xa5'*60)
 elif a==0x4ffa80:
  assert read(sp+4)==base+0x500;pop=4
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_EIP,read(sp));m.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,hook)
def call(a,args):
 u.mem_write(stack,w(stop)+args);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(a,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
levels=json.loads((root/'artifacts/ambient-records.json').read_text())['results']
seed=dict(levels[0]['records'][0]);special=[]
for uid in (0,0,0xffffffff,0xffffffff,777):
 r=dict(seed);r['uid']=uid;special.append(r)
groups=[l['records'] for l in levels]+[special]
cases=rows=0
for records in groups:
 for mode in range(3):
  allocations=[];registrations=[];u.mem_write(0x644ec0,w(0x644ec0,0x644ec0));u.mem_write(0x644f00,w(0))
  u.mem_write(0x17543d8,bytes([mode!=2]));command=bytearray(w(len(records)))
  for i,r in enumerate(records):
   sample=-1 if mode==2 else (-2147483648 if mode==1 and i%3==0 else i+88)
   name=r['name'].encode('cp1252');parameters=f(r['near_distance'],r['volume'],r['rolloff'])
   u.mem_write(base+0x500,name+b'\0');u.mem_write(base+0x400,f(*r['position']))
   before=len(allocations)
   returned=call(0x45aca0,w(r['uid'],base+0x500,base+0x400)+parameters+w(r['flags']))
   assert returned==(r['uid'] if sample>=0 else 0xffffffff)
   assert len(allocations)==before+(sample>=0)
   command.extend(struct.pack('<5I3f256s3f',r['uid'],r['header_byte'],r['flags'],r['offset'],r['bytes'],*r['position'],name,r['near_distance'],r['volume'],r['rolloff'])+w(sample))
  assert len(registrations)==(0 if mode==2 else len(records))
  assert read(0x644f00)==len(allocations)
  original=bytearray(w(len(allocations),len(records)-len(allocations)))
  for i,address in enumerate(allocations):
   assert read(address)==(allocations[i+1] if i+1<len(allocations) else 0x644ec0)
   assert read(address+4)==(allocations[i-1] if i else 0x644ec0)
   raw=bytes(u.mem_read(address,60));original.extend(raw[8:32]+raw[40:60])
  for r in records:
   found=call(0x45afe0,w(r['uid']));original.extend(w(allocations.index(found) if found else -1))
  actual=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--ambient-instances'],input=command)
  assert actual==original,(cases,mode)
  cases+=1;rows+=len(records)
report=dict(result='PASS',cases=cases,rows=rows,original_sha256=digest,scope='Full45aca0 constructor,45b080 setup,505a90 audio gate and45afe0 ordered lookup. Allocation/string ownership and5054b0 registration result supplied; actual vector/timer code and list insertion execute. Shared C state/list order matches accepted, negative registration and disabled-audio paths, including duplicate/zero/FFFFFFFF UIDs. No real device, nonempty registration internals or playback proof.')
(root/'artifacts/ambient-instances.json').write_text(json.dumps(report,indent=2));print(report)
