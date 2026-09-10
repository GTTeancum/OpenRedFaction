"""Original 4faaf0 vs shared particle direction preparation."""
import runpy,struct,random,math,re,hashlib,json,subprocess
from pathlib import Path
import pefile
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_definitions.py')));root=ctx['root'];x=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop']
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));image=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image);u.mem_map(base,0x20000)
entry=int(re.search(r'_rf_particle_definition_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
records=[f[2] for f in ctx['fixtures'][:77]];rng=random.Random(497590)
vectors=[(0,0,0),(0,-0.0,0),(1e-40,0,0),(1e38,1e38,1e38),(float('inf'),0,1),(float('nan'),2,3)]
vectors += [tuple(rng.randint(-10000,10000)/17 for _ in range(3)) for _ in range(512)]
records += [bytes([0xa5])*12+struct.pack('<3f',*v)+bytes([0xa5])*160 for v in vectors]
original=[];native=[]
for record in records:
 u.mem_write(base,record[12:24]);u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base);u.emu_start(0x4faaf0,stop,count=10000)
 assert u.reg_read(UC_X86_REG_EIP)==stop;original.append(record[:12]+bytes(u.mem_read(base,12))+record[24:])
 x.mem_write(base,record);x.mem_write(stack,struct.pack('<3I',stop,base,base));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0;native.append(bytes(x.mem_read(base,184)))
probe=ctx['probe'];pc=subprocess.check_output([str(probe),'--particle-prepare'],input=b''.join(records));exact=[0,0];maximum=0
for index,want in enumerate(original):
 for target,got in enumerate((pc[index*184:(index+1)*184],native[index])):
  assert got[:12]==want[:12] and got[24:]==want[24:]
  exact[target]+=got==want
  for a,b in zip(struct.unpack('<3f',got[12:24]),struct.unpack('<3f',want[12:24])):
   if math.isnan(b):assert math.isnan(a)
   else:
    assert math.isfinite(a) and abs(a-b)<=1e-6,(index,a,b)
    maximum=max(maximum,abs(a-b))
report=dict(result='PASS',cases=len(records),original_sha256=sha,pc_bit_exact=exact[0],nxdk_bit_exact=exact[1],max_error=maximum,scope='Unmodified original 4faaf0, including all 77 authored directions, zero, subnormal, large and nonfinite vectors. PC/NXDK within 1e-6 with NaN classification; all other metadata exact. No resource loading or active emitters.')
(root/'artifacts/particle-prepare-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
