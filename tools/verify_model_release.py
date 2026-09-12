"""Compare shared release dispatch with original resource boundaries and NXDK."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
inputs=[];expected=[]
def observe(g):
 kind=g['kind'];present=g['present'];attached=g['attached'];payload=123 if present else 0;materials=77 if attached else 0;trace=[]
 inputs.append(w(kind,payload,materials))
 if any(t[0]=='submodel' for t in g['trace']):trace.extend((1,kind,payload));payload=0
 if ('free',g['array']-4) in g['trace']:trace.extend((2,materials,payload))
 trace.extend((3,payload,materials));expected.append(w(0)+b'\xdd'*12+w(len(trace)//3,*trace,*([0]*(12-len(trace)))))
runpy.run_path(str(root/'tools/inspect_model_release.py'),init_globals={'observe_case':observe})
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--release'],input=b''.join(inputs));assert pc==b''.join(expected)
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);b=0x30000000;u.mem_map(b,0x10000)
backend=b+0x100;stub=b+0x200;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'\s_rf_model_release\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];retired=False

def hook(cpu,address,size,data):
 global retired
 if address not in (stub,stub+16,stub+32):return
 sp=cpu.reg_read(UC_X86_REG_ESP);assert read(sp+4)==1234
 if address==stub:trace.extend((1,read(sp+8),read(sp+12)));cpu.mem_write(b+4,w(456))
 elif address==stub+16:trace.extend((2,read(sp+8),read(b+4)))
 else:
  assert read(sp+8)==b;trace.extend((3,read(b+4),read(b+8)));cpu.mem_write(b,b'\xdd'*12);retired=True
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
def memory(cpu,access,address,size,value,data):
 assert not(retired and address<b+12 and address+size>b),('read after recycle',hex(address))
u.hook_add(UC_HOOK_CODE,hook);u.hook_add(UC_HOOK_MEM_READ,memory);u.mem_write(stub,b'\xc3'*48);u.mem_write(backend,w(stub,stub+16,stub+32,1234))
for i,(raw,want) in enumerate(zip(inputs,expected)):
 retired=False;trace.clear();u.mem_write(b,raw);u.mem_write(stack,w(stop,b,backend));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and retired
 got=w(u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(b,12))+w(len(trace)//3,*trace,*([0]*(12-len(trace))))
 assert got==want,(i,got.hex(),want.hex())
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Original/PC/NXDK type dispatch and callback ordering, post-payload clear, unchanged material token, recycler invalidation and no owner reads afterward. Resource destruction/recycle remain supplied port backends; original material destructor separately executes in the reference harness. No live scene/XEMU binding.')
(root/'artifacts/model-release-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
