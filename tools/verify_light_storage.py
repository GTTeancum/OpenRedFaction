"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



HEAP=0x31000000;x.mem_map(HEAP,4096);alloc=[];freed=[];fail=False
malloc=sym('malloc');free=sym('free')
def heap(cpu,address,size,ctx):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,arg=struct.unpack('<2I',cpu.mem_read(sp,8))
 if address==malloc:alloc.append(arg);cpu.reg_write(UC_X86_REG_EAX,0 if fail else HEAP)
 elif arg:freed.append(arg)
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,heap,begin=malloc,end=malloc);x.hook_add(UC_HOOK_CODE,heap,begin=free,end=free)
rng=random.Random(0x4ee210);inputs=[];responses=[];budget=324
for case in range(512):
 mappings=[rng.choice([0xffffffff,*range(16)]) for _ in range(8)];order=list(range(8));rng.shuffle(order);faces=[];expected=[]
 for i in range(8):
  lo=[rng.uniform(-10,0) for _ in range(3)];hi=[v+rng.uniform(0,10) for v in lo];flags=rng.randrange(1<<32);prop=rng.choice([-32768,-1,0,1,32767]);bounds=f(*lo,*hi)
  faces.append(bytes(16)+bounds+w(0,0,0,flags,prop&0xffffffff,0,0,0));expected.append(bounds+w(flags,prop&0xffffffff,mappings[order[i]]))
 data=w(*mappings,*order)+b''.join(faces);assert len(data)==640;inputs.append(data);response=w(0,8,16,budget)+b''.join(expected)+b'\x01'*16;responses.append(response)
 x.mem_write(B,data);x.mem_write(OWNER,bytes(68));x.mem_write(OWNER,w(UV));x.mem_write(OWNER+24,w(8));x.mem_write(OWNER+32,w(16));x.mem_write(OWNER+56,w(FACE));x.mem_write(FACE,w(*(i*56 for i in range(8))));x.mem_write(UV,bytes(448))
 for i,mapping in enumerate(mappings):x.mem_write(UV+i*56+20,w(mapping))
 x.mem_write(OUT,w(0));alloc.clear();freed.clear()
 assert call('rf_visibility_light_storage_open',[OWNER,B+64,B+32,8,budget-1,OUT])!=0 and not alloc and read(x,OUT)==0
 assert call('rf_visibility_light_storage_open',[OWNER,B+64,B+32,8,budget,OUT])==0 and alloc==[budget] and read(x,OUT)==HEAP
 assert w(0)+bytes(x.mem_read(HEAP+8,12))+bytes(x.mem_read(HEAP+20,304))==response
 x.mem_write(B,b'\xa5'*640);x.mem_write(OWNER,b'\xa5'*68);x.mem_write(UV,b'\xa5'*448)
 assert w(0)+bytes(x.mem_read(HEAP+8,12))+bytes(x.mem_read(HEAP+20,304))==response
 call('rf_visibility_light_storage_close',[OUT]);call('rf_visibility_light_storage_close',[OUT]);assert freed==[HEAP] and read(x,OUT)==0
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--light-storage'],input=b''.join(inputs))==b''.join(responses)
# Allocation failure and a late invalid source index leave output NULL.
x.mem_write(B,data);x.mem_write(OWNER,bytes(68));x.mem_write(OWNER,w(UV));x.mem_write(OWNER+24,w(8));x.mem_write(OWNER+32,w(16));x.mem_write(OWNER+56,w(FACE));x.mem_write(UV,bytes(448))
for i,mapping in enumerate(mappings):x.mem_write(UV+i*56+20,w(mapping))
fail=True;freed.clear();assert call('rf_visibility_light_storage_open',[OWNER,B+64,B+32,8,budget,OUT])!=0 and read(x,OUT)==0 and not freed
fail=False;x.mem_write(B+32+7*4,w(8));freed.clear();assert call('rf_visibility_light_storage_open',[OWNER,B+64,B+32,8,budget,OUT])!=0 and read(x,OUT)==0 and freed==[HEAP]
report=dict(result='PASS',pc_nxdk_cases=512,faces=4096,nxdk_failure_guards=2,scope='Shared geometry accessor and owner with supplied heap; reordered retained collision faces, exact/short budgets, copied metadata, initial dirty1, source destruction and repeated close. Mapping/sign and initial dirty semantics have static original loader evidence; not an original allocator/runtime-owner comparison or installed-level replay.')
(root/'artifacts/light-storage.json').write_text(json.dumps(report,indent=2));print(report)
