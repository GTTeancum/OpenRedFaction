"""Original activation eligibility, actor backlink and motion transition prefix."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
base=0x30000000;stack=base+0xe000;stop=base+0xf000
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);return u
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=machine(original);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_group_activation_begin\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.hook_add(UC_HOOK_CODE,lambda m,a,s,c:m.emu_stop(),begin=0x46acb2,end=0x46acb2)
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c];rng=random.Random(0x46aba0)
commands=bytearray();expected=bytearray();started_count=backlinks=active_backlinks=0
for n in range(4096):
 count=rng.randrange(0,9);flags=rng.randrange(0x4000);mode=n%6
 current=rng.randrange(max(1,count));next_key=rng.choice([-1,-1,0,1,2])
 state=w(flags,mode,current,next_key,0x3e800000,-1)
 present=n%5!=0;entity=present and n%3!=0;actorflags=rng.choice([0,8,0x4000,0x4008,0x80000000])
 actor=w(present,actorflags,entity,0x55555555);wire=w(count,0x10000)+state+actor
 before=bytearray(0x400);before[0x24:0x28]=w(8);before[0x2c:0x30]=w(0x10000);before[0x29c:0x2a0]=w(count)
 for i,o in enumerate(offsets):before[o:o+4]=state[i*4:i*4+4]
 u.mem_write(base,bytes(before));actor_before=bytearray(0x800)
 actor_before[0x24:0x28]=w(0 if entity else 4);actor_before[0x2c:0x30]=w(0x20001)
 actor_before[0x7c:0x80]=w(actorflags);actor_before[0x6cc:0x6d0]=w(0x55555555)
 u.mem_write(base+0x1000,bytes(actor_before));u.mem_write(0x7394cc,bytes(4096));u.mem_write(0x7394cc,w(base,base+0x1000 if present else 0))
 u.mem_write(stack,w(stop,0x10000,123,0x20001));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x46aba0,stop,count=10000);pc=u.reg_read(UC_X86_REG_EIP);assert pc in (stop,0x46acb2)
 started=int(pc==0x46acb2);after=bytes(u.mem_read(base,0x400));state_after=b''.join(after[o:o+4] for o in offsets)
 for o in offsets:before[o:o+4]=after[o:o+4]
 assert bytes(before)==after,(n,'controller mutation')
 backlink=bytes(u.mem_read(base+0x1000+0x6cc,4));actor_after=bytes(u.mem_read(base+0x1000,0x800));actor_before[0x6cc:0x6d0]=backlink
 assert bytes(actor_before)==actor_after,(n,'actor mutation')
 want=w(0)+state_after+actor[:12]+backlink+w(started)
 changed=backlink!=actor[12:];started_count+=started;backlinks+=changed;active_backlinks+=changed and next_key!=-1
 commands.extend(wire);expected.extend(want)
 x.mem_write(base,wire);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base+8,count,0x10000,base+32,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+8,40))+bytes(x.mem_read(base+0x100,4));assert got==want,('NXDK',n,got.hex(),want.hex())
# Port guards preserve the complete state, actor view and started sentinel.
for count,state,actor in [
 (0x80000000,w(2,1,0,-1,0,0),w(1,0,1,77)),
 (2,w(2,1,0,-1,0,0),w(0,0,1,77)),
 (2,w(2,1,2,-1,0,0),w(1,0,1,77))]:
 wire=w(count,0x10000)+state+actor;want=w(-4)+state+actor+w(0xa5a5a5a5)
 commands.extend(wire);expected.extend(want)
 x.mem_write(base,wire);x.mem_write(base+0x100,w(0xa5a5a5a5));x.mem_write(stack,w(stop,base+8,count,0x10000,base+32,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+8,40))+bytes(x.mem_read(base+0x100,4));assert got==want,('NXDK guard',got.hex(),want.hex())
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-activation-begin'],input=commands);assert actual==expected
assert started_count and backlinks and active_backlinks
report=dict(result='PASS',cases=4096,port_guards=3,started=started_count,backlinks=backlinks,active_backlinks=active_backlinks,original_sha256=digest,scope='Original 46aba0 and unchanged registry/type/actor/count helpers execute through the pre-sound boundary 46acb2 or normal early return. Full controller/actor mutation checked. Exact PC/NXDK prefix, including actor flag4000 rejection, zero/one-key gates and entity backlink before active no-op. Valid controller lookup supplied; sound/player/wakeup tail and live scene activation excluded.')
(root/'artifacts/group-activation-begin-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
