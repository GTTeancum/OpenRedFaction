"""Original pain timers/RNG for the registered guard's two-hit fixture."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
folder=root/'artifacts/npc-damage-binding';lines=(folder/'pc.txt').read_text().splitlines()
observed=next(list(map(int,l.split()[1:])) for l in lines if l.startswith('NPC_PAIN_TEST '))
decl=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--action',str(root/'Installed_Game/tables.vpp'),'env_guard','','flinch_stand'])
assert decl[:4]==bytes(4) and not decl[68:132].split(b'\0')[0]
name=decl[4:68].split(b'\0')[0].decode().split('.')[0]+'.rfa'
with (root/'Installed_Game/motions.vpp').open('rb') as f:
 count=struct.unpack('<4I',f.read(16))[2];at=2048+((count*64+2047)//2048)*2048
 for i in range(count):
  f.seek(2048+i*64);row=f.read(64);size=struct.unpack_from('<I',row,60)[0]
  if row[:60].split(b'\0')[0].decode().lower()==name.lower():f.seek(at);header=f.read(80);break
  at+=(size+2047)//2048*2048
 else:raise AssertionError(name)
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def put(a,*v):u.mem_write(a,w(*v))
put(b+0x2a0,b,-1,-1);put(b+0x828,-1);put(0x5a3ed8,1000);put(b+0xd014,1)
put(b+0x294,b+0x4000) # Class owner is valid; player association remains NULL.
for i in range(45):put(b+0xa54+i*16,-1)
put(b+0xa54+22*16,0);put(b+0xa54+23*16,0)
put(b+0x80,b+0x6000);put(b+0x6000,2,b+0x6100);put(b+0x6100+0x1d50,b+0x9000)
put(b+0x9000+0xf5c,b+0xb000);put(b+0xb078,b+0xc000);u.mem_write(b+0xc000,header)
starts=[];draws=0
def boundary(m,address,size,ctx):
 global draws
 if address not in (0x428d10,0x428c90,0x577eef):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<6I',m.mem_read(sp,24));value=0
 if address==0x428c90:starts.append(a[2:6]);assert a[2:6]==(22,0x3f800000,0,1)
 if address==0x577eef:value=b+0xd000;draws+=1
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,boundary);u.reg_write(UC_X86_REG_FPCW,0x27f)
expected=[]
for hit in range(2):
 put(stack,stop,b);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x428740,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 expected.extend(struct.unpack('<I',u.mem_read(b+o,4))[0] for o in (0x514,0x744,0x830,0x828,0xd014))
assert observed==expected,(observed,expected)
assert len(starts)==draws==1
report=dict(result='PASS',pain_words=expected,motion=name,tick_span=struct.unpack_from('<2i',header,16),starts=len(starts),rng_draws=draws,
 scope='Original428740 with real flags, combat, invalid-weapon reset, timer/RNG and loaded duration callees. Actual archive header. Active-fire queries and action-start boundary supplied; CRT thread pointer supplied. Two calls match PC retained deadlines/action/RNG; second call cooldown-rejected. Prepared unarmed NPC state, no audio/AI/armed behavior claim.')
(folder/'pain-report.json').write_text(json.dumps(report,indent=2));print(report)
