"""Original eight-slot surface reset/tick against reconstructed PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<f',v)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;pool=b;nodes=b+0x1000
original=root/'Installed_Game/RF.exe';native=root/'build/xbox/main.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)//4096*4096);u.mem_write(base,data);u.mem_map(b,65536);return u
o=machine(original);x=machine(native)
symbols=(root/'build/xbox/main.map').read_text()
reset=int(re.search(r'\s_rf_corpse_surface_reset\s+([0-9a-fA-F]+)',symbols)[1],16)
tick=int(re.search(r'\s_rf_corpse_surface_tick\s+([0-9a-fA-F]+)',symbols)[1],16)
def run(u,entry,args=b''):
 u.mem_write(stack,w(stop)+args);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
def get(u,a):return struct.unpack('<I',u.mem_read(a,4))[0]
def payload(u,base):return b''.join(bytes(u.mem_read(base+i*84,76)) for i in range(8))
def normalized(u,base,heads):
 pointers=[get(u,a) for a in heads]+[get(u,base+i*84+j) for i in range(8) for j in (76,80)]
 assert all(v==0 or base<=v<base+8*84 and (v-base)%84==0 for v in pointers)
 return w(*[1+(v-base)//84 if v else 0 for v in pointers])
rng=random.Random(42190);inputs=[];outputs=[]
for case in range(288):
 count=case%9;dt=[0,1/60,0.25,10,1000,2**-24,2**-100,65536][(case//9)%8]
 raw=[f(rng.choice([0,5,8,100000,2**24,rng.random()*500]))+rng.randbytes(72) for _ in range(8)]
 for u,base in [(o,0x62f490),(x,nodes)]:
  for i in range(8):u.mem_write(base+i*84,raw[i]+rng.randbytes(8))
 o.mem_write(0x62f488,w(0xdeadbeef));o.mem_write(0x62f764,w(0xdeadbeef))
 run(o,0x42dbb0);assert run(x,reset,w(pool,nodes))==0 and get(x,pool+8)==8
 reset_result=normalized(o,0x62f490,[0x62f488,0x62f764])+payload(o,0x62f490)
 assert reset_result==normalized(x,nodes,[pool,pool+4])+payload(x,nodes)
 assert payload(o,0x62f490)==b''.join(raw)
 for u,base,head in [(o,0x62f490,0x62f764),(x,nodes,pool+4)]:
  u.mem_write(head,w(base if count else 0))
  for i in range(count):u.mem_write(base+i*84+76,w(base+((i+1)%count)*84,base+((i+count-1)%count)*84))
 o.mem_write(0x5a4014,f(dt));run(o,0x42e190);assert run(x,tick,w(pool)+f(dt))==0
 expected=payload(o,0x62f490);assert expected==payload(x,nodes),(case,'NXDK tick')
 for i in range(8):
  value=struct.unpack('<f',raw[i][:4])[0]
  assert expected[i*76:(i+1)*76]==(f(value+struct.unpack('<f',f(dt))[0]) if i<count else raw[i][:4])+raw[i][4:]
 inputs.append(w(count)+f(dt)+b''.join(raw));outputs.append(reset_result+expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-surface-pool'],input=b''.join(inputs))
assert actual==b''.join(outputs),'PC reset/tick differs'
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(native.read_bytes()).hexdigest(),scope='Unhooked original42dbb0 reset and42e190 tick vs PC/NXDK. Exact reset ring order and preserved payloads, all active counts0..8, elapsed increments including large ages and tiny steps. Tick changes elapsed only. No draw/growth, live dispatch, automatic expiration or XEMU run claimed.')
(root/'artifacts/corpse-surface-pool.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
