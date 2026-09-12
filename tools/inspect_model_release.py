"""Execute original model release wrappers, auxiliary-array deletion and pool recycle.

Type-specific submodel destructors are supplied boundaries; real material
element destruction, scalar deleting wrappers, reverse traversal and model pool
recycling execute original instructions. No live model backend is claimed.
"""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(0,4096)
b=0x30000000;u.mem_map(b,0x20000);owner=b;resource=b+0x1000;array=b+0x4000;stack=b+0x1e000;stop=b+0x1f000;pool=0x173c3f0
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v));read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];retired=False;freed=[];material_payloads={}
def code(cpu,address,size,data):
 if address not in (0x51b070,0x54b570,0x54a8a0,0x57360e):return
 sp=cpu.reg_read(UC_X86_REG_ESP);ecx=cpu.reg_read(UC_X86_REG_ECX);ret=read(sp)
 if address==0x57360e:
  token=read(sp+4);trace.append(('free',token));assert token not in freed;freed.append(token)
  if token==resource:cpu.mem_write(token,b'\xdd'*256)
  elif token==array-4:cpu.mem_write(token,b'\xdd'*(4+200*count))
  else:assert token in material_payloads;cpu.mem_write(token,b'\xdd'*16)
 elif address==0x54a8a0:
  assert array<=ecx<array+count*200 and (ecx-array)%200==0
  trace.append(('auxiliary',(ecx-array)//200));assert read(owner+4)==(0 if present and kind in (2,3) else resource if present else 0)
  return # Observe entry, then execute the complete original material destructor.
 else:
  assert ecx==resource;trace.append(('submodel',2 if address==0x51b070 else 3))
  assert read(owner)==kind and read(owner+4)==resource
  # Outer code must clear the pointer after the deleting wrapper finishes.
  cpu.mem_write(owner+4,w(resource+128))
 cpu.reg_write(UC_X86_REG_EIP,ret);cpu.reg_write(UC_X86_REG_ESP,sp+4)
def write(cpu,access,address,size,value,data):
 global retired
 if address==owner:
  assert value==old_head;retired=True
  assert read(pool+0x157c0)==owner

def memory(cpu,access,address,size,value,data):
 assert not(retired and address<owner+88 and address+size>owner),('owner read after recycle',hex(address))
u.hook_add(UC_HOOK_CODE,code);u.hook_add(UC_HOOK_MEM_WRITE,write);u.hook_add(UC_HOOK_MEM_READ,memory)
rng=random.Random(0x502b10);cases=0;elements=0;submodels=0
for kind in (0,1,2,3,4,0xffffffff):
 for present in (0,1):
  for attached in (0,1):
   for count in range(6):
    retired=False;trace.clear();freed.clear();raw=bytearray(rng.randbytes(88));raw[:8]=w(kind,resource if present else 0);raw[80:84]=w(array if attached else 0)
    u.mem_write(owner,bytes(raw));u.mem_write(resource,bytes(256));u.mem_write(array-4,w(count)+bytes(200*count))
    material_payloads.clear()
    for j in range(count):
     u.mem_write(array+j*200+4,w(cases&1))
     for k,offset in enumerate((0x80,0xbc,0xc4)):
      token=b+0x8000+j*128+k*32
      if cases&(2<<k):
       u.mem_write(array+j*200+offset,w(token));u.mem_write(token,bytes(16));material_payloads[token]=(j,k)
    old_head=0 if cases%2 else b+0xc000;free_count=900+cases%50;live_count=1000-free_count
    u.mem_write(pool+0x157c0,w(old_head));u.mem_write(pool+0x157cc,w(free_count));u.mem_write(pool+0x157d8,w(live_count))
    u.mem_write(0,w(0));u.mem_write(stack,w(stop,owner));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x502b10,stop,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and retired
    expected=[]
    if present and kind in (2,3):expected.extend([('submodel',kind),('free',resource)]);raw[4:8]=w(0);submodels+=1
    if attached:
     for j in reversed(range(count)):
      expected.append(('auxiliary',j))
      if cases&1:
       for k in range(3):
        token=b+0x8000+j*128+k*32
        if token in material_payloads:expected.append(('free',token))
     expected.append(('free',array-4));elements+=count
    assert trace==expected,(kind,present,attached,count,trace,expected)
    raw[:4]=w(old_head);assert bytes(u.mem_read(owner,88))==raw
    assert read(pool+0x157c0)==owner and read(pool+0x157cc)==free_count+1 and read(pool+0x157d8)==live_count-1
    if 'observe_case' in globals():observe_case(globals())
    cases+=1
report=dict(result='PASS',cases=cases,submodels=submodels,auxiliary_elements=elements,original_sha256=sha,scope=__doc__)
(root/'artifacts/model-release-original.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
