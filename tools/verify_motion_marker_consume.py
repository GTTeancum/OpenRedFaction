"""Execute original51c420 marker polling; compare C state and consumed event."""
import hashlib,json,struct,subprocess,sys,random,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_EIP
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe=pefile.PE(str(original));u=Uc(UC_ARCH_X86,UC_MODE_32);im=pe.get_memory_mapped_image()
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x10000);obj=base;desc=base+0x3000;records=base+0x6000;name_at=base+0x8000;stack=base+0xe000;stop=base+0xf000
rng=random.Random(0x51c420);cases=[]
def fixed(s):return s.encode().ljust(16,b'\0')
for k in range(512):
 state=bytearray(260);n=k%5;struct.pack_into('<I',state,0,n)
 for i in range(n):struct.pack_into('<iif',state,4+i*12,(i+k)%4,160,1)
 dominant=-1 if n==0 else (k//5)%n
 if k%17==0:dominant=-7
 struct.pack_into('<3i',state,196,-1,-1,dominant);struct.pack_into('<I',state,252,1);struct.pack_into('<I',state,256,k%4)
 names=b''.join(fixed(a)+fixed(b) for a,b in [('footstep_left','footstep_right'),('same','same'),('','footstep_left'),('Footstep_left','other')])
 request=fixed(rng.choice(['footstep_left','footstep_right','Footstep_left','same','other','','absent']))
 cases.append(bytes(state)+names+request)
invalid=[]
for offset,value in [(0,17),(204,16),(4,4)]:
 b=bytearray(cases[1]);struct.pack_into('<i',b,offset,value);invalid.append(bytes(b))
b=bytearray(cases[1]);b[-16:]=b'x'*16;invalid.append(bytes(b))
run=subprocess.run([str(root/'build/pc/Release/rf_motion_file_probe.exe'),'--consume-marker'],input=b''.join(cases+invalid),capture_output=True,check=True)
assert len(run.stdout)==len(cases+invalid)*268
fired_count=0
for k,raw in enumerate(cases):
 state=raw[:260];mask=struct.unpack_from('<I',state,256)[0]
 u.mem_write(obj,bytes(0x2000));u.mem_write(obj+0x12d0,state[:196]);u.mem_write(obj+0x1d48,state[204:208]);u.mem_write(obj+0x1d50,struct.pack('<I',desc))
 u.mem_write(obj+0x1d44,bytes([mask&1,(mask>>1)&1]))
 for i in range(4):
  record=records+i*256;u.mem_write(desc+0xf5c+i*4,struct.pack('<I',record));u.mem_write(record+0x40,raw[260+i*32:276+i*32]);u.mem_write(record+0x54,raw[276+i*32:292+i*32])
 u.mem_write(name_at,raw[-16:]);u.mem_write(stack,struct.pack('<2I',stop,name_at));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,obj)
 u.emu_start(0x51c420,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=bytearray(state);bits=u.mem_read(obj+0x1d44,2);struct.pack_into('<I',expected,256,bits[0]|bits[1]<<1)
 fired=u.reg_read(UC_X86_REG_EAX)&255;fired_count+=fired
 result=run.stdout[k*268:(k+1)*268];assert result==struct.pack('<iI',0,fired)+expected,(k,result.hex(),expected.hex())
for k,raw in enumerate(invalid,len(cases)):
 assert run.stdout[k*268:(k+1)*268]==struct.pack('<iI',-4,0xdeadbeef)+raw[:260],k
nxpe=pefile.PE(str(root/'build/xbox/main.exe'));nximage=nxpe.get_memory_mapped_image();nx=Uc(UC_ARCH_X86,UC_MODE_32)
nxbase=nxpe.OPTIONAL_HEADER.ImageBase;nx.mem_map(nxbase,(len(nximage)+4095)//4096*4096);nx.mem_write(nxbase,nximage)
nx.mem_map(base,0x10000)
entry=int(re.search(r'_rf_motion_consume_marker\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for k,raw in enumerate(cases+invalid):
 nx.mem_write(obj,raw);nx.mem_write(obj+0x1000,struct.pack('<I',0xdeadbeef))
 nx.mem_write(stack,struct.pack('<6I',stop,obj,obj+260,4,obj+388,obj+0x1000));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,stop,count=10000);assert nx.reg_read(UC_X86_REG_EIP)==stop
 result=struct.pack('<I',nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(obj+0x1000,4))+bytes(nx.mem_read(obj,260))
 assert result==run.stdout[k*268:(k+1)*268],('NXDK',k)
report=dict(result='PASS',original_sha256=digest,cases=len(cases),fired=fired_count,invalid=len(invalid),scope='Unchanged original51c420 AL result and event-bit mutation versus PC and NXDK machine code; dominant-resource mapping, exact case, empty/missing/duplicate names, all two-bit masks. Bounds failures preserve state/output. Sound dispatch excluded.')
(root/'artifacts/motion-marker-consume.json').write_text(json.dumps(report,indent=2));print(report)
