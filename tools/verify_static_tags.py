"""Original503220/501220/53c23f static prefix lookup versus shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def emulator(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)&~4095);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
 u.mem_map(0x30000000,0x10000);return u
original=root/'Installed_Game/RF.exe'
assert hashlib.sha256(original.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=emulator(original);x=emulator(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_model_find_static_tag\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
B=0x30000000;S=B+0xf000;STOP=B+0xff00;N=B+0x8000;Q=B+0x9000;OUT=B+0xa000
pool=[b'corona_1',b'CORONA_10',b'corona_rod1',b'corona_rod2',b'light_prop',b'',b'\xc0',b'\xe0']
rng=random.Random(0x53c23f)
cases=[([b'corona_10',b'corona_1'],b'CORONA_1'),([b''],b''),([],b''),([b'x'],b'xy')]
cases += [([rng.choice(pool) for _ in range(rng.randrange(17))],rng.choice(pool+[b'corona_',b'COR',b'missing'])) for _ in range(512)]
wire=bytearray();results=[]
for names,query in cases:
 # Static wrapper -> first submesh -> LOD owner -> authored attachment array.
 o.mem_write(B,w(1,B+0x100));o.mem_write(B+0x148,w(1,B+0x200))
 o.mem_write(B+0x28c,w(B+0x300));o.mem_write(B+0x304,w(B+0x400))
 o.mem_write(B+0x410,w(B+0x1000,len(names)));o.mem_write(0x20852f4,w(0))
 for i,name in enumerate(names):
  o.mem_write(B+0x1000+100*i,name+b'\0')
  x.mem_write(N+i*8,w(B+0x1000+100*i,len(name)));x.mem_write(B+0x1000+100*i,name+b'\0')
 for u in (o,x):u.mem_write(Q,query+b'\0')
 o.mem_write(S,w(STOP,B,Q));o.reg_write(UC_X86_REG_ESP,S);o.emu_start(0x503220,STOP,count=100000)
 assert o.reg_read(UC_X86_REG_EIP)==STOP
 want=o.reg_read(UC_X86_REG_EAX);x.mem_write(OUT,w(-1))
 x.mem_write(S,w(STOP,N,len(names),Q,len(query),OUT));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=struct.unpack('<I',x.mem_read(OUT,4))[0];status=x.reg_read(UC_X86_REG_EAX)
 assert actual==want and (status==0)==(want!=0xffffffff),(names,query,want,status,actual)
 results.append(w(status,actual))
 wire+=w(len(names),0,0)+b''.join(n.ljust(32,b'\0') for n in names)+bytes((48-len(names))*32)+query.ljust(32,b'\0')
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--static-tag'],input=wire)
assert pc==b''.join(results)
report=dict(result='PASS',cases=len(cases),scope='Unhooked original static wrapper503220/501220/53c23f and default-locale comparator5829a0 versus shared PC and compiled NXDK. Supplied first-submesh attachment arrays; duplicates, prefix/exact/missing/empty/non-ASCII names. No loader ownership, other wrapper kinds, glare effects or native XEMU claim.')
(root/'artifacts/static-tags.json').write_text(json.dumps(report,indent=2));print(report)
