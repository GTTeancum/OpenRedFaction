"""Compare reconstructed PC/NXDK surface effects to full original construction."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(data)+4095)//4096*4096);x.mem_write(base,data)
b=0x30000000;x.mem_map(b,65536);pool=b;source=b+0x100;nodes=[b+0x1000+i*84 for i in range(3)]
backend=b+0x2000;callbacks=b+0x2100;name=b+0x2200;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
get=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
put=lambda a,v:x.mem_write(a,w(v))
entry=int(re.search(r'\s_rf_corpse_surface_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
current={};inputs=[];outputs=[]

def hook(m,address,size,unused):
 if not callbacks<=address<callbacks+80 or (address-callbacks)%16:return
 op=(address-callbacks)//16;sp=m.reg_read(UC_X86_REG_ESP);result=0
 if op==0:result=1
 elif op==1:result=4
 elif op==2:m.mem_write(get(sp+16),bytes(12))
 elif op==3:
  m.mem_write(get(sp+16),current['hit']+w(1));put(get(sp+20),int(current['matched']))
 elif op==4:put(get(sp+16),0xff123456)
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,get(sp))
x.hook_add(UC_HOOK_CODE,hook)

def compare(c):
 global current
 current=c;x.mem_write(b,bytes(0x3000));x.mem_write(name,b'eye\0')
 put(source,0x30003000);put(source+4,1)
 x.mem_write(backend,w(*[callbacks+16*i for i in range(5)],0))
 x.mem_write(pool,w(nodes[0],nodes[1] if c['existing'] else 0,3))
 for index,next_index in [(0,2 if c['free_count']==2 else 0),(1,1),(2,0)]:
  put(nodes[index]+76,nodes[next_index]);put(nodes[index]+80,nodes[next_index])
 x.mem_write(stack,w(stop,pool,source,1,name)+struct.pack('<2f',c['growth'],c['extent'])+w(backend))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 def normalize(values,addresses):return [addresses.index(a)+1 if a else 0 for a in values]
 expected=w(0)+c['payload']+w(*normalize(c['links'],c['addresses']))
 links=[get(a) for a in [pool,pool+4]+[node+offset for node in nodes for offset in [76,80]]]
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(nodes[0],76))+w(*normalize(links,nodes))
 assert actual==expected,('NXDK',len(inputs),[(i,a,z) for i,(a,z) in enumerate(zip(actual,expected)) if a!=z])
 inputs.append(w(c['free_count'],c['existing'])+struct.pack('<2f',c['growth'],c['extent'])+w(int(c['matched']))+c['hit'])
 outputs.append(expected)

runpy.run_path(str(root/'tools/verify_corpse_source_surface.py'),init_globals={'observe_case':compare})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-surface'],input=b''.join(inputs))
expected=b''.join(outputs)
assert actual==expected,('PC',[(i,a,z) for i,(a,z) in enumerate(zip(actual,expected)) if a!=z][:20],len(actual),len(expected))
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='Original42dc50 vs shared PC/NXDK construction: exact76-byte payload and normalized three-node ring links. Original executes flat/sloped room geometry; shared surface callback consumes its hit. Shared attachment, geometry and face-color backends remain supplied; recycling/lookup failure and live rendering not compared by this test.')
(root/'artifacts/corpse-surface-shared.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
