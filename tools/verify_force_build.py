"""Prepared original box construction blocks vs authored PC/NXDK construction."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
base=0x30000000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
    p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(origin,(len(b)+4095)//4096*4096);m.mem_write(origin,b);m.mem_map(base,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u,x=machine(exe),machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_physics_force_region_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inventory=json.loads((root/'artifacts/force-regions.json').read_text());commands=bytearray();expected=bytearray();counts=[0,0,0]
for level in inventory['results']:
 for r in level['records']:
    shape=r['shape'];counts[shape-1]+=1;matrix=r['orientation_disk'][3:]+r['orientation_disk'][:3]
    source=w(r['uid'],r['offset'],r['bytes'],r['header_byte'],shape,r['flags'])+bytes(512)+f(*r['position'],*r['orientation_disk'],*(r['extent']+[0]*(3-len(r['extent']))),r['strength'])
    initial=w(shape,r['uid'],r['flags'])+f(*r['position'],*matrix,r['extent'][0] if shape==1 else 0,*([0]*6),*(r['extent'] if shape!=1 else [0]*3),r['strength'])+w(1)
    u.mem_write(base,initial);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base)
    u.reg_write(UC_X86_REG_EBP,base+12);u.reg_write(UC_X86_REG_EBX,base+24 if shape==3 else base+88)
    u.reg_write(UC_X86_REG_FPCW,0x37f)
    if shape==1:
        u.reg_write(UC_X86_REG_EAX,struct.unpack('<I',f(r['extent'][0]))[0]);u.reg_write(UC_X86_REG_ECX,stack+0x6c)
    u.emu_start(0x46306a if shape==3 else 0x4630d8 if shape==2 else 0x463167,0x4631b1,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x4631b1
    result=bytes(u.mem_read(base,108));commands.extend(source);expected.extend(result)
    x.mem_write(base,source);x.mem_write(base+0x1000,bytes([0xa5])*108)
    x.mem_write(stack,w(stop,base,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    actual=bytes(x.mem_read(base+0x1000,108))
    assert actual==result,(level['file'],r['uid'],[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--force-build'],input=commands)
assert actual==expected,'PC differs'
report=dict(result='PASS',shape_counts=counts,original_sha256=digest,
 scope='All authored records: prepared original 46306a/4630d8/463167 through 4631b1 with unchanged vector/bounds callees; all 108 PC/NXDK runtime bytes agree. Reader and prior field writes supplied; allocation, registration and force application excluded.')
(root/'artifacts/force-build.json').write_text(json.dumps(report,indent=2));print(report)
