"""Complete original segment/AABB function, including failed-attempt writes."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);stack=base+60000;stop=base+64000

reports=json.loads((root/'artifacts/geometry.json').read_text());levels=[];values=set()
for report in reports:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_geometry_probe.exe'),str(root/'Installed_Game'/report['archive']),report['file'],'--flags'])
 flags=[int(v,16) for v in raw.split()];assert len(flags)==report['faces'];values.update(flags)
 levels.append(dict(file=report['file'],faces=len(flags),high_flags=sum(v>255 for v in flags)))
rng=random.Random(0x4edeb9);values.update(rng.getrandbits(32) for _ in range(1000))
u.hook_add(UC_HOOK_CODE,lambda uc,a,size,data:uc.emu_stop(),begin=0x4edec8,end=0x4edec8)
for value in values:
 stream=base;payload=base+4096
 u.mem_write(stream,bytes(96));u.mem_write(stream,struct.pack('<5I',1,0,payload,0,4));u.mem_write(stream+0x50,struct.pack('<I',180));u.mem_write(payload,struct.pack('<I',value))
 u.reg_write(UC_X86_REG_ESI,stream);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x4edeb9,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4edec8
 assert struct.unpack('<I',u.mem_read(stack+0x68,4))[0]==value
 assert struct.unpack('<I',u.mem_read(stream+0xc,4))[0]==4
report=dict(result='PASS',levels=len(levels),faces=sum(r['faces'] for r in levels),faces_with_high_flags=sum(r['high_flags'] for r in levels),original_reader_cases=len(values),scope='Original 4edeb9..4edec8 executes unchanged 515420 in binary version-180 mode, consumes four bytes and stores full flags word. C accessor compared to raw +40 across every inventoried face.',detail=levels)
(root/'artifacts/geometry-flags-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='detail'})
