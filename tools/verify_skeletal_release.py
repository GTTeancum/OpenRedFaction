"""Verify skeletal retirement against full original51b070 on PC and NXDK."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
inputs=[];expected=[]
def observe(g):
 size,victim,enabled=g['size'],g['victim'],g['enabled'];u=g['u'];nodes=g['nodes'];a=nodes[victim]
 index=lambda p:nodes.index(p) if p else 0xffffffff
 inputs.append(w(size,victim,enabled)+g['before_active']+w(*g['refs']))
 links=[]
 for i in range(4):links.extend([index(g['read'](nodes[i]+o)) if i<size else 0xffffffff for o in (0x1d54,0x1d58)])
 expected.append(w(0,index(g['read'](g['head'])),enabled,*links)+bytes(u.mem_read(a+0x12d0,196))+bytes(u.mem_read(a+0x1cfc,8))+bytes(u.mem_read(a+0x1d48,4))+w(*g['expected_refs']))
runpy.run_path(str(root/'tools/inspect_skeletal_release.py'),init_globals={'observe_case':observe})
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--skeletal-release'],input=b''.join(inputs));assert pc==b''.join(expected)
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);b=0x30000000;u.mem_map(b,0x10000)
active=b+0x1000;head=b+0x2000;resources=b+0x3000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'\s_rf_model_skeletal_retire\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
index=lambda p:(p-b)//16 if p else 0xffffffff
snapshot=lambda:bytes(u.mem_read(b,64))+bytes(u.mem_read(active,208))+bytes(u.mem_read(head,4))+bytes(u.mem_read(resources,16*36))
def call(node,limit):
 u.mem_write(stack,w(stop,node,head,limit,resources,16));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
short=bad=repeat=0
for i,(raw,want) in enumerate(zip(inputs,expected)):
 size,victim,enabled=struct.unpack_from('<3I',raw);node=b+16*victim
 u.mem_write(b,bytes(64));u.mem_write(active,raw[12:220]);u.mem_write(head,w(b));u.mem_write(resources,bytes(16*36))
 for j in range(16):u.mem_write(resources+36*j+32,raw[220+4*j:224+4*j])
 for j in range(size):u.mem_write(b+16*j,w(enabled if j==victim else 0,active if j==victim else 0,b+16*((j+1)%size),b+16*((j+size-1)%size)))
 before=snapshot()
 if enabled:
  assert call(node,size-1)==0xfffffffc and snapshot()==before;short+=1
  if read(active):
   saved=bytes(u.mem_read(active+4,4));u.mem_write(active+4,w(99));bad_before=snapshot()
   assert call(node,4)==0xfffffffc and snapshot()==bad_before;bad+=1;u.mem_write(active+4,saved)
 assert call(node,4)==0
 links=[]
 for j in range(4):links.extend(index(read(b+16*j+o)) for o in (8,12))
 got=w(0,index(read(head)),read(node),*links)+bytes(u.mem_read(active,208))+w(*[read(resources+36*j+32) for j in range(16)])
 assert got==want,(i,got.hex(),want.hex())
 if enabled:
  after=snapshot();assert call(node,4)!=0 and snapshot()==after;repeat+=1
report=dict(result='PASS',cases=len(inputs),short_ring_limits=short,invalid_motion_ids=bad,repeated_retirement=repeat,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full original51b070 versus PC/NXDK active-slot bytes, reference counts and normalized ring links/head. Original removal/decrement callees run unchanged. Port malformed-input guards checked on compiled NXDK. No live model registry/scene binding or cache free.')
(root/'artifacts/skeletal-release-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
