"""Shared UID resolution vs unchanged original object/key-owner lookups."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(data)+4095)//4096*4096);u.mem_write(base,data)
 u.mem_map(0x30000000,0x40000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_level_link_resolve\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
base=0x30000000;stack=base+0x30000;stop=stack+0x1000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
def call(m,address,args):
 m.mem_write(stack,pack(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=100000)
 assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
rng=random.Random(4611);wire=bytearray();expected=[]
for case in range(2000):
 uid=rng.choice([0xffffffff,0xfffffc19,0,1,2,3,4,100])
 objects=[(rng.choice([uid,0xffffffff,0xfffffc19,1,2,3]),0x12340000+i,rng.randrange(4)) for i in range(rng.randrange(9))]
 keys=[(rng.choice([uid,0xffffffff,0xfffffc19,1,2,3]),0x23450000+i) for i in range(rng.randrange(9))]
 for i,(ident,handle,flags) in enumerate(objects):
  a=base+i*0x400
  u.mem_write(a+0x10,pack(a+0x400 if i+1<len(objects) else 0x73d880));u.mem_write(a+0x20,pack(ident));u.mem_write(a+0x2c,pack(handle));u.mem_write(a+0x7c,pack(flags))
 u.mem_write(0x73d890,pack(base if objects else 0x73d880))
 for i,(ident,handle) in enumerate(keys):
  a=base+0x4000+i*0x400;k=base+0x8000+i*16
  u.mem_write(a+0x28c,pack(a+0x400 if i+1<len(keys) else 0x64e3b0));u.mem_write(a+0x29c,pack(1,1,k));u.mem_write(k,pack(k+4,ident));u.mem_write(a+0x2c,pack(handle))
 u.mem_write(0x64e63c,pack(base+0x4000 if keys else 0x64e3b0))
 obj=call(u,0x48a4a0,[uid])
 if obj: want=(struct.unpack('<I',u.mem_read(obj+0x2c,4))[0],1,(obj-base)//0x400)
 else:
  owner=call(u,0x46afc0,[uid])
  want=(struct.unpack('<I',u.mem_read(owner+0x2c,4))[0],2,(owner-base-0x4000)//0x400) if owner else (uid,0,0xffffffff)
 ob=b''.join(pack(*o) for o in objects);kb=b''.join(pack(*k) for k in keys)
 wire+=pack(uid,len(objects),len(keys))+ob+kb;expected.append(pack(*want))
 if ob:x.mem_write(base,ob)
 if kb:x.mem_write(base+0x1000,kb)
 assert call(x,entry,[uid,base,len(objects),base+0x1000,len(keys),base+0x2000])==0
 assert bytes(x.mem_read(base+0x2000,12))==expected[-1],(case,'NXDK')
raw=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),'--resolve-links'],input=wire)
assert raw==b''.join(expected),'PC'
report=dict(result='PASS',cases=len(expected),scope='PC and compiled NXDK vs unchanged original 48a4a0/46afc0. Ordered duplicates, object-first precedence, missing UIDs, -1 key fallback, -999 object flag filtering. Synthetic ordered registries; no ownership, registration or entity backlinks.')
(root/'artifacts/uid-resolution-verification.json').write_text(json.dumps(report,indent=2));print(report)
