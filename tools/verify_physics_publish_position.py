"""Completed physics-position publication against unchanged original calls."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
base=0x30000000;stack=base+0x8000;stop=base+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');rng=random.Random(0x487962)
entry=int(re.search(r'\s_rf_physics_publish_position\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
cases=[];expected=[]
mapping=[(88,0xe4,12),(100,0xf0,12),(244,0x180,4),(248,0x190,24),(272,0x1a8,4),(308,0x3c,12),(320,0x7c,4)]
for n in range(1024):
 raw=bytearray(rng.randbytes(324));struct.pack_into('<3f',raw,88,*(rng.uniform(-10000,10000) for _ in range(3)))
 struct.pack_into('<f',raw,244,[0.,-0.,-2.,rng.uniform(.01,100)][n%4]);cases.append(bytes(raw))
 obj=bytearray(rng.randbytes(0x300))
 for dst,src,size in mapping:obj[src:src+size]=raw[dst:dst+size]
 u.mem_write(base,bytes(obj));u.mem_write(stack,pack(base+0xe4));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,base)
 u.mem_write(0x64ecb9,bytes(2));u.emu_start(0x487962,0x487973,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==0x487973 and u.reg_read(UC_X86_REG_ESP)==stack+4
 after=bytes(u.mem_read(base,len(obj)));want=bytearray(raw)
 for dst,src,size in mapping:want[dst:dst+size]=after[src:src+size];obj[src:src+size]=after[src:src+size]
 assert bytes(obj)==after
 expected.append(pack(0)+want)
# Explicit port finite-contract failures, including a late overflowing bound.
for offset,bits in [(88,0x7fc00000),(244,0x7f800000),(96,0x7f800000),(244,0x7f7fffff)]:
 raw=bytearray(cases[3]);struct.pack_into('<I',raw,offset,bits)
 if bits==0x7f7fffff:struct.pack_into('<I',raw,96,bits)
 cases.append(bytes(raw));expected.append(pack(0xfffffffc)+raw)
for n,(raw,want) in enumerate(zip(cases,expected)):
 nx.mem_write(base,raw);nx.mem_write(stack,pack(stop,base,base+308,base+320));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,stop,count=10000);assert nx.reg_read(UC_X86_REG_EIP)==stop
 got=pack(nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base,324));assert got==want,('NXDK',n,got.hex(),want.hex())
pc=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--publish-position'],input=b''.join(cases));assert pc==b''.join(expected)
report=dict(result='PASS',original_cases=1024,port_failures=4,original_sha256=sha,scope='Prepared487962..487973 including complete original48a230 and vector/bounds callees, debug naming disabled. Exact PC/NXDK body/public position/flags; positive,zero,negative radius, aliased original body source, arbitrary untouched state. Four finite-contract errors preserve all outputs. Body completion scheduling, list removal and linked-object traversal excluded.')
(root/'artifacts/physics-publish-position.json').write_text(json.dumps(report,indent=2));print(report)
