"""Original two-tag glare center arithmetic, without tag/allocator stubs."""
import runpy,struct,re,random,subprocess,json
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,B,S,STOP=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(int(v)&0xffffffff for v in v))
entry=int(re.search(r'_rf_glare_segment_center\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(414016);fixtures=[]
for first in (0,1,-1,16777216,-16777216,1e20):
 for second in (0,1,-1,16777216,-16777216,1e20):fixtures.append([first]*3+[second]*3)
fixtures += [[rng.uniform(-1e6,1e6) for _ in range(6)] for _ in range(1000)]
commands=bytearray();results=bytearray();different=0
for values in fixtures:
 command=struct.pack('<6f',*values);commands.extend(command);u.mem_write(S+0x10,command)
 u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x414016,0x414049,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x414049
 expected=bytes(4)+bytes(u.mem_read(u.reg_read(UC_X86_REG_EAX),12));results.extend(expected)
 floats=struct.unpack('<6f',command)
 average=struct.pack('<3f',*[struct.unpack('<f',struct.pack('<f',floats[i]+floats[i+3]))[0]*.5 for i in range(3)])
 different+=average!=expected[4:]
 x.mem_write(B,command);x.mem_write(B+0x1000,bytes([0xa5])*12);x.mem_write(S,w(STOP,B,B+12,B+0x1000));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x1000,12))==expected,values
assert subprocess.check_output([str(c['probe']),'--glare-segment-center'],input=commands)==results
assert different>0
guards=[]
for i in range(6):
 for invalid in (float('nan'),float('inf')):
  v=[0]*6;v[i]=invalid;guards.append(v)
guards.append([-3e38,0,0,3e38,0,0])
for values in guards:
 command=struct.pack('<6f',*values);x.mem_write(B,command);x.mem_write(B+0x1000,bytes([0xa5])*12)
 x.mem_write(S,w(STOP,B,B+12,B+0x1000));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 expected=w(-2)+bytes([0xa5])*12
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+0x1000,12))==expected
 assert subprocess.check_output([str(c['probe']),'--glare-segment-center'],input=command)==expected
report=dict(result='PASS',cases=len(fixtures),different_from_sum_average=different,guards=len(guards),scope='Original414016..414049 with actual409fa0 subtraction,40a070 multiplication,40a030 addition; exact PC/NXDK midpoint. Invalid/overflow guards are shared policy. Two-tag lookup, allocation/initialization and view-owner assignment excluded.')
(root/'artifacts/glare-segment-center.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
