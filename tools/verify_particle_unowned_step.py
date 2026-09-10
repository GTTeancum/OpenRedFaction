"""Unowned emitter-linked particle simulation and bounds against original 495120."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_creation.py')))
u=c['u'];x=c['x'];base=c['base'];stack=c['stack'];stop=c['stop'];root=c['root'];put=c['put'];get=c['get']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
u.reg_write(UC_X86_REG_FPCW,0x27f);x.reg_write(UC_X86_REG_FPCW,0x27f)
entry=int(re.search(r'_rf_particle_pool_step_unowned\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000);storage=base+0x10000;pool=base+0x4000;lists=base+0x5000
node=base+0x2000;active=base+0x5000;free=0x7a3b08
commands=bytearray();expected=bytearray();rng=random.Random(495120);expired=0;expanded=0;emitter=base+0x6000
for case in range(2048):
 p=bytearray(120);struct.pack_into('<3I',p,0,1605,1605,0xffffffff)
 velocity=tuple(rng.randint(-1000,1000)/64 for _ in range(3)) if case%17 else (0,0,0)
 struct.pack_into('<6f',p,12,*(rng.randint(-1000,1000)/64 for _ in range(3)),*velocity)
 age=(case%9)/8;life=(case%7+1)/4;dt=(0,0.015625,0.25,1)[(case//16)%4]
 struct.pack_into('<f3I5f',p,0x24,age,rng.getrandbits(32),rng.getrandbits(32),0xa5a5a5a5,life,(case%5)/4,(case%11-5)/4,(case%13-6)/4,9.8)
 flags=1|((case%16)&1)*4|((case%16>>1)&1)*8|((case%16>>2)&1)*64|((case%16>>3)&1)*0x8000
 struct.pack_into('<I',p,0x58,flags);struct.pack_into('<I',p,0x64,0x1234)
 struct.pack_into('<I',p,0x68,1)
 bound_owner=(-1,0,1)[case%3];center=tuple(rng.randint(-1000,1000)/64 for _ in range(3));maximum=(0,1,100,10000)[case%4]
 bounds=struct.pack('<i4f',bound_owner,*center,maximum)
 u.mem_write(emitter,bytes(0x158));put(emitter+4,bound_owner&0xffffffff)
 u.mem_write(emitter+0xa0,struct.pack('<f',maximum));u.mem_write(emitter+0xa4,struct.pack('<3f',*center))
 original=bytearray(p);struct.pack_into('<I',original,0x68,emitter);struct.pack_into('<II',original,0,active,active)
 u.mem_write(node,bytes(original));put(active,node);put(active+4,node);put(free,free);put(free+4,free);put(0x7a3bf8,1)
 u.mem_write(stack,struct.pack('<IIf',stop,active,dt));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x495120,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 result=bytearray(u.mem_read(node,120));live=get(0x7a3bf8);expired+=1-live
 for off in (0,4):struct.pack_into('<I',result,off,{active:1605,free:1600}[struct.unpack_from('<I',result,off)[0]])
 struct.pack_into('<I',result,0x68,1)
 result_bounds=bounds[:16]+bytes(u.mem_read(emitter+0xa0,4));expanded+=result_bounds!=bounds
 output=struct.pack('<iI',0,live)+result+result_bounds;expected.extend(output);commands.extend(struct.pack('<f',dt)+p+bounds)
 x.mem_write(storage,bytes(p));x.mem_write(pool,struct.pack('<5I',storage,lists,6,1,0));x.mem_write(base+0x7000,bounds);x.mem_write(lists,struct.pack('<12I',1600,1600,1601,1601,1602,1602,1603,1603,1604,1604,0,0));x.mem_write(stack,struct.pack('<IIIfI',stop,pool,0,dt,base+0x7000));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(pool+12,4))+bytes(x.mem_read(storage,120))+bytes(x.mem_read(base+0x7000,20))
 assert actual==output,(case,[(hex(i),a,b) for i,(a,b) in enumerate(zip(actual,output)) if a!=b])
actual_pc=subprocess.check_output([str(c['ctx']['probe']),'--particle-step-unowned'],input=commands)
assert len(actual_pc)==len(expected)
assert actual_pc==expected,[(i//148,i%148,a,b) for i,(a,b) in enumerate(zip(actual_pc,expected)) if a!=b][:20]
report=dict(result='PASS',cases=2048,expired=expired,bounds_expanded=expanded,scope='Full original 495120 with actual 4973e0 and vector distance helpers, emitter-linked negative-owner particles versus PC/NXDK. Exact particle payload, count and emitter bounds; collision/swirl/wind/damage and nonnegative particle ownership remain excluded.')
(root/'artifacts/particle-unowned-step-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
