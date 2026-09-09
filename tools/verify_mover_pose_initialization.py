"""Original mover pose-copy block and complete object position assignment."""
import hashlib,json,random,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,65536);obj=base;params=base+4096;source=base+8192;stack=base+60000;stop=base+64000
records=[r for l in json.loads((root/'artifacts/movers.json').read_text())['results'] for r in l['records']]
for r in records:
 disk=r['orientation_disk'];matrix=struct.pack('<9f',*(disk[3:]+disk[:3]));position=struct.pack('<3f',*r['position'])
 # 49f010 receives object+88 and factory parameters. Execute pose copy block
 # including its real vector/matrix callees, stopping before momentum setup.
 initial=bytes([0xa5])*0x298;u.mem_write(obj,initial);u.mem_write(params,bytes(0x98));u.mem_write(params+0x3c,position);u.mem_write(params+0x48,matrix)
 u.reg_write(UC_X86_REG_ESI,obj+0x88);u.reg_write(UC_X86_REG_EDI,params);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x49f051,0x49f0ab,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49f0ab
 want=bytearray(initial)
 for offset in [0xe4,0xf0]:want[offset:offset+12]=position
 for offset in [0xfc,0x120]:want[offset:offset+36]=matrix
 want[0x144:0x150]=bytes(12);want[0x168:0x174]=bytes(12)
 assert bytes(u.mem_read(obj,len(want)))==want,(r['uid'],'physics pose copy')
 # Factory base pose copies: matrix -> +48/+244, position -> +238.
 u.reg_write(UC_X86_REG_ESI,obj);u.reg_write(UC_X86_REG_EDI,params+0x3c);u.reg_write(UC_X86_REG_EBP,params);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x486ee6,0x486f0f,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x486f0f
 want[0x238:0x244]=position;want[0x244:0x268]=matrix;want[0x48:0x6c]=matrix
 assert bytes(u.mem_read(obj,len(want)))==want,(r['uid'],'factory base pose')
rng=random.Random(0x48a230);cases=[]
for r in records:
 for radius in [-1,0,.25,1,10]:cases.append((r['position'],radius))
for i in range(1000):cases.append(([rng.uniform(-1e6,1e6) for _ in range(3)],rng.uniform(0,100)))
u.mem_write(0x64ecb9,bytes(2));aliases=0;port_cases=[];port_expected=[]
def mapped(blob):
 return b''.join(blob[offset:offset+size] for offset,size in [(0x7c,4),(0x180,4),(0x238,12),(0x244,36),(0xe4,12),(0x3c,12),(0xf0,12),(0x144,12),(0x48,36),(0xfc,36),(0x120,36),(0x190,12),(0x19c,12)])
for n,(position,radius) in enumerate(cases):
 position=list(struct.unpack('<3f',struct.pack('<3f',*position)));radius=struct.unpack('<f',struct.pack('<f',radius))[0];pose=struct.pack('<3f',*position)
 initial=bytearray(bytes([0xa5])*0x298);struct.pack_into('<f',initial,0x180,radius);struct.pack_into('<I',initial,0x7c,0x12345678)
 ptr=source if n%2 else obj+0xf0
 if ptr!=source:initial[0xf0:0xfc]=pose;aliases+=1
 u.mem_write(obj,bytes(initial));u.mem_write(ptr,pose);u.mem_write(stack,struct.pack('<2I',stop,ptr));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,obj);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x48a230,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 port_cases.append(mapped(initial)+pose+struct.pack('<I',int(ptr!=source)))
 want=bytearray(initial)
 for offset in [0x3c,0xe4,0xf0]:want[offset:offset+12]=pose
 want[0x190:0x19c]=struct.pack('<3f',*[p-radius for p in position]) if radius>0 else pose
 want[0x19c:0x1a8]=struct.pack('<3f',*[p+radius for p in position]) if radius>0 else pose
 struct.pack_into('<I',want,0x7c,0x12345678|0x4000000)
 assert bytes(u.mem_read(obj,len(want)))==want,(n,position,radius,'position assignment')
 port_expected.append(mapped(bytes(u.mem_read(obj,len(want)))))
report=dict(result='PASS',physics_pose_blocks=len(records),factory_base_pose_blocks=len(records),position_assignments=len(cases),aliased_sources=aliases,scope='Original 49f051..49f0ab verifies physics pose copies; 486ee6..486f0f verifies factory position +238 and matrices +48/+244 for every file mover pose with unchanged helpers. Complete unmodified 48a230 with debug lookup disabled verifies all object bytes, positive/nonpositive radius bounds, position copies and dirty flag, including source alias at f0. This is not complete factory/physics construction.')
(root/'artifacts/mover-pose-initialization.json').write_text(json.dumps(report,indent=2));print(report)
