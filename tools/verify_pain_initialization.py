"""Verify constructor-owned flinch deadlines; not complete actor construction."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im)
obj=0x30000000;cls=obj+0x4000;stack=obj+0xe000;u.mem_map(obj,65536)
calls=[]
def sound_boundary(m,address,size,context):
 if address not in (0x434d00,0x5056a0):return
 calls.append(address);sp=m.reg_read(UC_X86_REG_ESP)
 ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 m.reg_write(UC_X86_REG_EAX,0x12345678);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,sound_boundary)
rng=random.Random(0x423354);commands=[];wanted=[]
for case in range(512):
 now=[0,1,1072799999,1072800000][case%4] if case<64 else rng.randrange(1072800001)
 before=bytes(rng.getrandbits(8) for _ in range(0x1500));u.mem_write(obj,before)
 u.mem_write(0x5a3ed8,w(now));u.mem_write(stack,w(0)*8)
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,obj+0x2a0)
 u.reg_write(UC_X86_REG_EAX,obj);u.reg_write(UC_X86_REG_EBX,0xdeadbeef)
 # Includes actual EBX clear, timer setters/clearers and scalar writes.
 u.emu_start(0x402c33,0x402d68,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x402d68
 state=bytes(u.mem_read(obj,0x1500))
 assert state[:0x2a0]==before[:0x2a0] and state[0x7a0:]==before[0x7a0:]
 assert struct.unpack_from('<I',state,0x514)[0]==now
 assert struct.unpack_from('<I',state,0x744)[0]==now
 # Later factory sets cooldown830 and retained action828. Exercise both
 # class sound branches; sound resolution/playback are explicit boundaries.
 u.mem_write(cls+0x13c,w(-1 if case%2==0 else 7));calls.clear()
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,obj)
 u.reg_write(UC_X86_REG_EBX,cls);u.reg_write(UC_X86_REG_EDI,0xffffffff)
 u.reg_write(UC_X86_REG_EBP,0xdeadbeef)
 u.emu_start(0x423318,0x4233a8,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4233a8
 expected=bytearray(state)
 for offset in (0x82c,0x83c,0x146c,0x844,0x808,0x824,0x828,0x838,0x13d4):expected[offset:offset+4]=w(-1)
 for offset in (0x830,0x136c):expected[offset:offset+4]=w(now)
 expected[0x13d8:0x13dc]=w(0);expected[0x80c:0x810]=w(-1 if case%2==0 else 0x12345678)
 assert bytes(u.mem_read(obj,0x1500))==expected
 assert calls==([] if case%2==0 else [0x434d00,0x5056a0])
 # Existing shared setter and both queries agree with the observed initial state.
 for op,result in ((3,123),(4,1),(7,0)):
  commands.append(w(now,0,0,now,op,0));wanted.append(w(0,now,0,0,now,result))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_timer_probe.exe')],input=b''.join(commands))
assert actual==b''.join(wanted)
report=dict(result='PASS',constructor_cases=512,pc_timer_checks=1536,
 scope='Original402c33..402d68 and423318..4233a8 with actual timer callees; two factory sound calls supplied. Poisoned owners and EBP/EBX; both sound branches. AI timer514, lock744 and cooldown830 equal construction time, action828 is-1. Exact complete post-factory object bytes. PC setter/expired/pending agree. Does not execute full actor construction or later AI/event writes.')
(root/'artifacts/pain-initialization.json').write_text(json.dumps(report,indent=2));print(report)
