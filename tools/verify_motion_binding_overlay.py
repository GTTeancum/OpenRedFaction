"""Original weapon mapping overlay versus PC/NXDK whole records."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;info=b+0x3000;states=b+0x5000;actions=b+0x6000;stack=b+0xe000;stop=b+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x20000);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_motion_overlay_bindings\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x42ab20);payload=[];expected=[]
for i in range(1024):
 counts=[rng.choice([-3,0,1,23]),rng.choice([-3,0,1,45])];flags=int(counts[0]>0)|int(counts[1]>0)<<1
 base=rng.randbytes(1088);weapon=bytearray(rng.randbytes(1088))
 for at in range(0,1088,16):struct.pack_into('<i',weapon,at,rng.choice([-1,-1,-2,0,7,1023,-2147483648]))
 payload.append(pack(flags)+base+weapon)
 raw=bytearray(rng.randbytes(0x2000));struct.pack_into('<I',raw,0x24,0);struct.pack_into('<I',raw,0x294,info)
 raw[0xd24:0xd24+1088]=base;struct.pack_into('<I',raw,0x1164+3*4,states);struct.pack_into('<I',raw,0x1264+3*4,actions)
 u.mem_write(b,bytes(raw));u.mem_write(info,bytes(0x1000));u.mem_write(info+0x94,pack(2))
 u.mem_write(info+0x768+3*4,struct.pack('<i',counts[0]));u.mem_write(info+0x868+3*4,struct.pack('<i',counts[1]))
 u.mem_write(states,bytes(weapon[:368]));u.mem_write(actions,bytes(weapon[368:]))
 u.mem_write(0x85cd00,pack(7));u.mem_write(0x85ccd8,pack(3))
 u.mem_write(stack,pack(stop,b,7 if i&1 else 3));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x42ab20,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 want=bytes(u.mem_read(b+0x8e4,1088));raw[0x8e4:0x8e4+1088]=want
 assert bytes(u.mem_read(b,0x2000))==raw
 expected.append(pack(0)+want)
 # Exercise supported result/base alias, independent external weapon arrays.
 x.mem_write(b,base);x.mem_write(states,bytes(weapon[:368]));x.mem_write(actions,bytes(weapon[368:]))
 x.mem_write(stack,pack(stop,b,b,states if flags&1 else 0,actions if flags&2 else 0))
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b,1088))==want,i
pc=subprocess.check_output([str(root/'build/pc/Release/rf_motion_probe.exe'),'--overlay-bindings'],input=b''.join(payload))
assert pc==b''.join(expected)
report=dict(result='PASS',cases=len(payload),original_sha256=digest,scope='Full original42ab20 and actual40a1e0 predicate execute for valid skeletal entities, including original weapon alias. Whole actor writes checked; all16-byte records equal PC/NXDK overlay composition. Negative/zero declared counts, absent groups, exact -1 sentinel and arbitrary auxiliary words covered. Port helper expects gating/alias already resolved; live switching excluded.')
(root/'artifacts/motion-binding-overlay.json').write_text(json.dumps(report,indent=2));print(report)
