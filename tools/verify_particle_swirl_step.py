"""Swirl update against full original 495120, including its CRT random draws."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_creation.py')))
u=c['u'];x=c['x'];base=c['base'];stack=c['stack'];stop=c['stop'];root=c['root'];put=c['put'];get=c['get']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x27f);x.reg_write(UC_X86_REG_FPCW,0x27f)
entry=int(re.search(r'_rf_particle_pool_step_random\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000);storage=base+0x10000;pool=base+0x4000;lists=base+0x5000
node=base+0x2000;active=base+0x5000;free=0x7a3b08
commands=bytearray();expected=bytearray();rng=random.Random(495120);expired=0
for case in range(4096):
 p=bytearray(120);struct.pack_into('<3I',p,0,1602,1602,0xffffffff)
 velocity=tuple(rng.randint(-1000,1000)/64 for _ in range(3)) if case%17 else (0,0,0)
 struct.pack_into('<6f',p,12,*(rng.randint(-1000,1000)/64 for _ in range(3)),*velocity)
 age=(case%9)/8;life=(case%7+1)/4;dt=(0,0.015625,0.25,1)[(case//16)%4]
 struct.pack_into('<f3I5f',p,0x24,age,rng.getrandbits(32),rng.getrandbits(32),0xa5a5a5a5,life,(case%5)/4,(case%11-5)/4,(case%13-6)/4,9.8)
 flags=(((case//16)%16)<<24)|1|((case%16)&1)*4|((case%16>>1)&1)*8|((case%16>>2)&1)*64|((case%16>>3)&1)*0x8000
 struct.pack_into('<I',p,0x58,flags);struct.pack_into('<I',p,0x64,0x1234)
 seed=rng.getrandbits(32);put(c['thread']+0x14,seed)
 original=bytearray(p);struct.pack_into('<II',original,0,active,active)
 u.mem_write(node,bytes(original));put(active,node);put(active+4,node);put(free,free);put(free+4,free);put(0x7a3bf8,1)
 u.mem_write(stack,struct.pack('<IIf',stop,active,dt));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x495120,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytearray(u.mem_read(node,120));live=get(0x7a3bf8);expired+=1-live
 for off in (0,4):struct.pack_into('<I',result,off,{active:1602,free:1600}[struct.unpack_from('<I',result,off)[0]])
 output=struct.pack('<iII',0,live,get(c['thread']+0x14))+result;expected.extend(output);commands.extend(struct.pack('<fI',dt,seed)+p)
 x.mem_write(storage,bytes(p));x.mem_write(pool,struct.pack('<5I',storage,lists,5,1,0));x.mem_write(lists,struct.pack('<10I',1600,1600,1601,1601,0,0,1603,1603,1604,1604));x.mem_write(base+0x6000,struct.pack('<I',seed));x.mem_write(stack,struct.pack('<IIIf5I',stop,pool,0,dt,0,0,0,0,base+0x6000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(pool+12,4))+bytes(x.mem_read(base+0x6000,4))+bytes(x.mem_read(storage,120))
 assert actual==output,(case,[(hex(i),a,b) for i,(a,b) in enumerate(zip(actual,output)) if a!=b])
# Rejected updates preserve both caller RNG and every particle byte.
rejections=0
for dt,flags in ((-0.25,0x02000001),(float('nan'),0x02000001),
                 (float('inf'),0x02000001),(0.25,0x12000001),
                 (0.25,0x02000011)):
 rejected=bytearray(p);struct.pack_into('<I',rejected,0x58,flags)
 status=-4 if not (dt>=0 and dt<float('inf')) else -3
 out=struct.pack('<iII',status,1,seed)+rejected
 commands.extend(struct.pack('<fI',dt,seed)+rejected);expected.extend(out)
 x.mem_write(storage,bytes(rejected));x.mem_write(pool,struct.pack('<5I',storage,lists,5,1,0))
 x.mem_write(base+0x6000,struct.pack('<I',seed))
 x.mem_write(stack,struct.pack('<IIIf5I',stop,pool,0,dt,0,0,0,0,base+0x6000))
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(pool+12,4))+bytes(x.mem_read(base+0x6000,4))+bytes(x.mem_read(storage,120))==out
 rejections+=1
actual_pc=subprocess.check_output([str(c['ctx']['probe']),'--particle-step-random'],input=commands)
assert len(actual_pc)==len(expected)
assert actual_pc==expected,[(i,a,b) for i,(a,b) in enumerate(zip(actual_pc,expected)) if a!=b][:20]
report=dict(result='PASS',cases=4096,expired=expired,rejections=rejections,scope='Full original 495120 against PC and NXDK shared swirl step: all 16 nibbles, zero and moving velocity, acceleration, gravity, color, expiry, RNG and complete records. Explicit 53-bit x87; no world collision or visual effect integration.')
(root/'artifacts/particle-swirl-input.bin').write_bytes(commands)
(root/'artifacts/particle-swirl-output.bin').write_bytes(expected)
(root/'artifacts/particle-swirl-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
