"""Trace original directional trigger geometry without replacing callees."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data)
 m.mem_map(base,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
from unicorn import UC_HOOK_CODE
u=machine(original)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
corners=[base+0x1000+i*16 for i in range(4)]
u.mem_write(0x85682c,w(*corners));calls=[]
def observe(m,address,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<6I',m.mem_read(sp,24))
 query=struct.unpack('<6f',m.mem_read(args[1],24));plane=struct.unpack('<4f',m.mem_read(args[3],16))
 pointers=struct.unpack('<3I',m.mem_read(args[2],12))
 vertices=[struct.unpack('<3f',m.mem_read(p,12)) for p in pointers]
 calls.append(dict(query=query,plane=plane,vertices=vertices))
u.hook_add(UC_HOOK_CODE,observe,begin=0x5065b0,end=0x5065b0)
def run(name,current,start,end):
 calls.clear();trigger=bytearray(0x400);actor=bytearray(0x200)
 struct.pack_into('<9f',trigger,0x48,1,0,0,0,1,0,0,0,1);struct.pack_into('<I',trigger,0x2b0,32);struct.pack_into('<3f',trigger,0x2c8,2,2,2)
 for offset,point in ((0x3c,current),(0xe4,start),(0xf0,end)):struct.pack_into('<3f',actor,offset,*point)
 u.mem_write(base,bytes(trigger));u.mem_write(base+0x400,bytes(actor));u.mem_write(stack,w(stop,base,base+0x400));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4c0a80,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=u.reg_read(UC_X86_REG_EAX)&255;assert result in (0,1)
 assert bytes(u.mem_read(base,0x400))==trigger and bytes(u.mem_read(base+0x400,0x200))==actor
 return dict(name=name,current=current,start=start,end=end,hit=result,calls=list(calls))
rows=[]
for y in (-.75,0,.75):
 for x in (-.75,0,.75):rows.append(run('grid',(x,y,2),(x,y,2),(x,y,0)))
rows.extend([run('reverse',(0,0,0),(0,0,0),(0,0,2)),run('stationary',(0,0,2),(0,0,2),(0,0,2)),run('distinct-origin',(0,0,2),(0,0,4),(0,0,0)),run('below-threshold',(0,0,0),(0,0,2),(0,0,-.000001)),run('above-threshold',(0,0,0),(0,0,2),(0,0,-.0001))])
assert [r['hit'] for r in rows]==[1,1,1,1,1,1,1,0,1,0,0,0,0,0]
assert [len(r['calls']) for r in rows]==[1,1,1,2,1,1,2,2,1,0,0,2,0,2]
assert rows[11]['calls'][0]['query']==(0,0,4,0,0,-2)
assert rows[7]['calls'][1]['vertices']==[(1,-1,1),(-1,-1,1),(-1,1,1)]
report=dict(original_sha256=digest,scope='Original 4c0a80 directional branch and all geometry callees unchanged. Four nonaliasing scratch allocations supplied as initialized by 4bf6d0. Read-only hook records 5065b0 inputs. Identity box diagnostic fixtures, 53-bit nearest x87. Not reconstructed code or native gameplay.',fixtures=rows)
(root/'artifacts/trigger-directional-original.json').write_text(json.dumps(report,indent=2)+'\n')
for r in rows:print(r['name'],r['current'],r['hit'],len(r['calls']),r['calls'][0] if r['calls'] else '')
