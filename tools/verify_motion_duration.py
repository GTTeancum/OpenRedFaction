"""Exact loaded character duration through original5033e0 and all callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
b=0x30000000;stack=b+0xe000;store=b+0xf000;output=b+0xf100
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im)
 m.mem_map(b,65536);m.mem_write(store,b'\xdd\x1d'+w(output));m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_motion_duration\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(b,w(2,b+0x100));u.mem_write(b+0x100+0x1d50,w(b+0x3000))
u.mem_write(b+0x3000+0xf5c+12,w(b+0x5000));u.mem_write(b+0x5000+0x78,w(b+0x6000))
rng=random.Random(0x51c2e0);commands=[];expected=[];different=0
edges=[0,1,159,160,4799,4800,4801,14400,0x7fffffff,0x80000000,0xffffffff]
for case in range(4096):
 start=rng.getrandbits(32);span=edges[case%len(edges)] if case<1024 else rng.getrandbits(32)
 end=(start+span)&0xffffffff
 u.mem_write(b+0x6000+16,w(start,end));u.mem_write(stack,w(store,b,3));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x5033e0,store+6,count=100000);assert u.reg_read(UC_X86_REG_EIP)==store+6
 original=bytes(u.mem_read(output,8));expected.append(original);commands.append(w(start,end))
 x.mem_write(stack,w(store,start,end));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,store+6,count=100000);assert x.reg_read(UC_X86_REG_EIP)==store+6
 assert bytes(x.mem_read(output,8))==original,('NXDK',case,start,end)
 direct=(span if span<=0x7fffffff else 0)/4800.0
 different+=original!=struct.pack('<d',direct)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_motion_probe.exe'),'--duration'],input=b''.join(commands))
assert pc==b''.join(expected),'PC duration mismatch'
report=dict(result='PASS',cases=4096,differ_from_direct_division=different,x87_control_word='0x027f',
 scope='Full original5033e0/501c60/51c2e0 with actual loaded539df0, tick getters and max helper; no substituted callees. Exact PC/NXDK double bytes including signed subtraction wrap, nonpositive spans and fractional durations. Loaded type2 only; allocation/missing clips and other model kinds excluded. Live pain integration remains separate.')
(root/'artifacts/motion-duration.json').write_text(json.dumps(report,indent=2));print(report)
