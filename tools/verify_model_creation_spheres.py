"""Original generic creation sphere block, static/cached animated, vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_EBP,UC_X86_REG_ESI
b=0x30000000;desc=b+0x4000;handle=b+0x5000;container=b+0x6000;source=b+0x9000;physics=b+0xa000;actor=b+0xb000;out=b+0xc000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
    m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
def run(m,a,args):
    m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(a,stop,count=100000);assert m.reg_read(UC_X86_REG_EIP)==stop;return m.reg_read(UC_X86_REG_EAX)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
binary=root/'build/xbox/main.exe';u=machine(exe);u.mem_map(0,4096);x=machine(binary)
entry=int(re.search(r'\s_rf_model_creation_spheres\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
params=out;heap=b+0xd000;allocations=[]
def allocate(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);n=struct.unpack('<I',cpu.mem_read(sp+4,4))[0];assert n==384 and not allocations
 allocations.append(n);cpu.mem_write(heap,b'\xa5'*n);cpu.reg_write(UC_X86_REG_EAX,heap);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,struct.unpack('<I',cpu.mem_read(sp,4))[0])
u.hook_add(UC_HOOK_CODE,allocate,begin=0x573619,end=0x573619)
rng=random.Random(0x486fc5);inputs=[];outputs=[]
for case in range(512):
 count=case%9;kind=1+(case//9)%2;bones=4;capacity=count+1
 matrices=f(*[rng.randrange(-8,9)/4 for _ in range(bones*12)])
 rows=[]
 for i in range(count):rows.append(bytes(28)+w(rng.choice((-1,0,1,2,3)))+f(*[rng.randrange(-20,21)/4 for _ in range(3)],rng.randrange(1,10)/4))
 models=b''.join(rows);raw=b''.join(row[:24]+row[28:] for row in rows)
 u.mem_write(b,matrices);u.mem_write(b+0x1d50,w(desc));u.mem_write(desc+0x48,w(bones))
 u.mem_write(handle,w(kind,b if kind==2 else container,container))
 u.mem_write(container+(0x19c0 if kind==2 else 0)+0x60,w(count,source))
 if raw:u.mem_write(source,raw)
 u.mem_write(actor+0x80,w(handle));u.mem_write(params,bytes(152));u.mem_write(stack,b'\xa5'*128)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,actor);u.reg_write(UC_X86_REG_EBP,params);allocations.clear()
 try:u.emu_start(0x486fc5,0x487040,count=1000000)
 except Exception:print(case,hex(u.reg_read(UC_X86_REG_EIP)),allocations);raise
 assert u.reg_read(UC_X86_REG_EIP)==0x487040
 read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
 assert read(params+136)==count and len(allocations)==int(count>0)
 initial=rng.randbytes(capacity*24);expected=bytearray(initial)
 for i in range(count):expected[i*24:i*24+16]=u.mem_read(read(params+144)+i*24,16)
 if count==1:assert expected[:12]==bytes(12)
 x.mem_write(source,models or bytes(48));x.mem_write(b,matrices);x.mem_write(physics,initial)
 status=run(x,entry,[source,count,kind,b,bones,physics,capacity]);assert status==0
 actual=bytes(x.mem_read(physics,len(initial)));assert actual==expected,(case,actual.hex(),expected.hex())
 inputs.append(w(count,kind,bones,capacity)+models+matrices+initial);outputs.append(w(0)+expected)
# Failure before writing and a later bad animated parent preserve untouched rows.
for guard in range(3):
 rows=[bytes(28)+w(-1)+f(1,2,3,.5),bytes(28)+w(8 if guard==2 else -1)+f(4,5,6,1)]
 models=b''.join(rows);count=2;kind=7 if guard==0 else 2;capacity=1 if guard==1 else 2
 initial=b'\xa5'*(capacity*24);x.mem_write(source,models);x.mem_write(physics,initial)
 status=run(x,entry,[source,count,kind,b,bones,physics,capacity]);assert status==0xfffffffc
 expected=bytearray(initial)
 if guard==2:expected[:16]=f(1,2,3,.5)
 assert bytes(x.mem_read(physics,len(initial)))==expected
 inputs.append(w(count,kind,bones,capacity)+models+matrices+initial);outputs.append(w(status)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--creation-spheres'],input=b''.join(inputs))==b''.join(outputs)
report=dict(result='PASS',original_cases=512,port_guards=3,original_sha256=sha,scope='Original486fc5..487040 only, actual503250/503270 static and cached animated queries plus array growth/copy. PC/NXDK center/radius bytes match, one-sphere origin rule, zero through8 rows, preserved caller auxiliary fields and untouched spare rows. Supplied allocator only; original temporary auxiliary words are unspecified. No full generic owner, fallback physics or native XEMU claim.')
(root/'artifacts/model-creation-spheres.json').write_text(json.dumps(report,indent=2));print(report)
