"""Original movement descriptor selection and creation flag adjustment."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');rng=random.Random(0x4339d0)
entry=int(re.search(r'\s_rf_movement_start\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=[];expected=[];fallback=cleared=0
for n in range(1536):
 table=bytearray(rng.randbytes(512))
 for i in range(16):struct.pack_into('<II',table,i*32,[0,1,2,255,256,257][(n+i)%6],(n+i)%17)
 requested=[-1,0,1,3,10,15,16,0x7fffffff][n%8];flags=rng.getrandbits(32)
 raw=bytes(table)+pack(requested&0xffffffff,flags);cases.append(raw)
 u.mem_write(0x62fe50,bytes(table));u.mem_write(stack,pack(stop,requested&0xffffffff));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4339d0,stop,count=100);assert u.reg_read(UC_X86_REG_EIP)==stop
 selected=(u.reg_read(UC_X86_REG_EAX)-0x62fe50)//32
 assert bytes(u.mem_read(0x630050,4))==pack(selected)
 fallback+=selected==0
 u.mem_write(base+0x1a8,pack(flags));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x422dfa,0x422e19,count=100);assert u.reg_read(UC_X86_REG_EIP)==0x422e19 and u.reg_read(UC_X86_REG_ESP)==stack+4
 assert bytes(u.mem_read(base+0x858,4))==pack(0x62fe50+selected*32)
 out=bytes(u.mem_read(base+0x1a8,4));cleared+=out!=pack(flags);want=pack(selected)+out;expected.append(want)
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base,requested&0xffffffff,base+516));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,stop,count=1000);assert nx.reg_read(UC_X86_REG_EIP)==stop
 assert pack(nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base+516,4))==want,('NXDK',n)
 assert bytes(nx.mem_read(base,516))==raw[:516]
pc=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--start'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(cases),slot_zero=fallback,changed_flags=cleared,original_sha256=sha,scope='Complete4339d0 and prepared422dfa..422e19; no hooks. PC/NXDK selected slot and body flags exact; disabled low-byte/out-of-range fallback, disabled slot0, descriptor index independent from slot, unrelated fields preserved. Name matching, other creation phases and later mode transitions excluded.')
(root/'artifacts/movement-start.json').write_text(json.dumps(report,indent=2));print(report)
