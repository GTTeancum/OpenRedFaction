"""PC/NXDK file-face surface callback, including output-preserving guards."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;x.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_geometry_body_surface\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
palette=bytearray(2548);struct.pack_into('<I',palette,240,3)
for i,(name,value) in enumerate([('rock',1),('metal',2),('ice',8)]):
 palette[244+i*36:244+i*36+len(name)]=name.encode();struct.pack_into('<I',palette,244+i*36+32,value)
x.mem_write(b+0x2000,bytes(palette));x.mem_write(b,w(b+0x100,3,b+0x200,b+0x2000));x.mem_write(b+0x100,w(b+0x400,b+0x400,b+0x400))
x.mem_write(b+0x200,w(0,8,0,0,0,b+0x300,b+0x320,3,0,0));x.mem_write(b+0x300,w(0,1,2,3))
x.mem_write(b+0x400,w(b+0x1000,160,0,1,0,0,1,0,0,0,0,0,b+0x500,0,b+0x504,0,0));x.mem_write(b+0x500,w(64,0))
cases=[]
for solid in (0xffffffff,0,1):
 for name,value in [('rock_wall',1),('METAL_door',2),('ice_floor',8),('ice',0),('iceberg_floor',0),('x'*63,0),('',0)]:
  cases.append((solid,0,0,0,name,0,[4,2,7][0 if solid==0xffffffff else solid+1],value))
 cases.append((solid,0,0xffffffff,0,'rock_wall',0,0xffffffff,0))
for solid,face,tex,slot in [(2,0,0,0),(0xfffffffe,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,8)]:
 cases.append((solid,face,tex,slot,'metal_door',-4 if solid in (2,0xfffffffe) or face else -2,0xa5a5a5a5,0xa5a5a5a5))
commands=bytearray();expected=bytearray()
for solid,face,tex,slot,name,status,texture,material in cases:
 commands.extend(w(solid,face,tex,slot)+name.encode().ljust(64,b'\0'));want=w(status,texture,material);expected.extend(want)
 data=bytearray(160);struct.pack_into('<I',data,16,tex);struct.pack_into('<H',data,64,len(name));data[66:66+len(name)]=name.encode()
 x.mem_write(b+0x1000,bytes(data));x.mem_write(b+0x320,w(*(slot for _ in range(3))) if slot else w(4,2,7));x.mem_write(b+0x3000,w(0xa5a5a5a5,0xa5a5a5a5))
 x.mem_write(stack,w(stop,b,solid,face,b+0x3000,b+0x3004));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x3000,8));assert got==want,('NXDK',solid,name,got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--body-surface'],input=commands)
assert actual==expected,('PC',actual.hex(),expected.hex())
report=dict(result='PASS',cases=len(cases),scope='PC and NXDK machine-code surface callback with synthetic loaded geometry and authored-style prefix table. World/mover slot mapping, case/prefix boundaries, no-texture default, invalid solid/face/texture/slot and unchanged error outputs. Existing original468740 verifier separately covers authored prefixes. No live scene integration.')
(root/'artifacts/body-surfaces-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
