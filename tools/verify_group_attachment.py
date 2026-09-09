"""C/NXDK attachment binding vs original loop with ordered object lookup."""
import json,math,random,re,runpy,struct,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_attachment.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
rng=random.Random(0x46b6e8);cases=[];expected=[]
for n in range(3000):
 count=rng.randrange(17);items=[(rng.choice([-999,-1,10,20,30,40]),rng.choice([8,9]),rng.choice([0,2,0x40000])) for _ in range(count)]
 ids=[rng.choice([0xffffffff,(-999)&0xffffffff,10,20,30,40,999]) for _ in range(rng.randrange(33))];flags=rng.choice([0,4,0x104,0x2004,0x1004,0x1000]);mode=n%2;controller=0x23450000+n
 o['initialize'](items);remaining,handles,rotation=o['attach'](ids,flags,controller,mode)
 objects=b''.join(struct.pack('<i4I',uid,kind,0x12340000+i,0xffffffff,f) for i,(uid,kind,f) in enumerate(items))+bytes((16-count)*20)
 refs=struct.pack('<32I',*(ids+[0xa5a5a5a5]*(32-len(ids))));handle_array=bytes([0xa5])*128
 cases.append(struct.pack('<7If',count,controller,flags,mode,len(ids),0,32,1)+objects+refs+handle_array)
 updated=[]
 for i,(uid,kind,f) in enumerate(items):
  parent,=struct.unpack('<I',o['u'].mem_read(o['objects']+i*1024+0x30,4));fl,=struct.unpack('<I',o['u'].mem_read(o['objects']+i*1024+0x7c,4));updated.append((uid,kind,0x12340000+i,parent,fl))
 expected.append((remaining,handles,rotation,b''.join(struct.pack('<i4I',*obj) for obj in updated)+bytes((16-count)*20)))
# Capacity failure must leave every output field intact, even with early valid references.
for capacity,rotation in [(0,1),(32,math.nan),(32,math.inf)]:
 wire=bytearray(cases[0]);struct.pack_into('<7If',wire,0,1,7,4,0,2,0,capacity,rotation);struct.pack_into('<i4I',wire,32,10,9,100,99,0);struct.pack_into('<2I',wire,352,10,10);cases.append(bytes(wire));expected.append(None)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--attach-movers'],input=b''.join(cases));assert len(raw)==592*len(cases)
def check(n,got):
 status,rc,hc,rotation=struct.unpack_from('<i2If',got);want=expected[n]
 if want is None:
  code=-4 if n==3000 else -2;assert status==code and got[4:]==cases[n][16:24]+cases[n][28:],(n,'guard');return
 refs,handles,r,objects=want;assert status==0 and rc==len(refs) and hc==len(handles) and struct.pack('<f',rotation)==struct.pack('<f',r)
 assert got[16:336]==objects,(n,'objects')
 assert list(struct.unpack_from('<'+'I'*rc,got,336))==refs and list(struct.unpack_from('<'+'I'*hc,got,464))==handles,(n,'lists')
for i in range(len(cases)):check(i,raw[i*592:i*592+592])
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(im)+4095)//4096*4096);x.mem_write(xb,im);base=0x30000000;x.mem_map(base,65536);stack=base+60000;stop=base+64000
entry=int(re.search(r'_rf_group_attach_movers\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,wire in enumerate(cases):
 count,controller,flags,mode,rc,hc,cap=struct.unpack_from('<7I',wire);x.mem_write(base,wire)
 args=[base+32,count,controller,flags,mode,base+352,base+16,base+480,base+20,cap,base+28]
 x.mem_write(stack,struct.pack('<12I',stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+16,8))+bytes(x.mem_read(base+28,580));check(i,got)
report=dict(result='PASS',original_cases=3000,port_guards=3,scope='Original mover-attachment loop vs PC and NXDK; randomized ordered objects including duplicate UIDs and -1/-999, missing/wrong types, duplicate references, parent/flag effects and rotation gates. Active lists compared; unused compacted tail unspecified. Guards preserve all state. No full controller creation, scene binding or playback.')
(root/'artifacts/group-attachment-verification.json').write_text(json.dumps(report,indent=2));print(report)
