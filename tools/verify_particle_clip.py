"""Original UV-only billboard polygon clipping versus bounded C on PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_billboard_clip\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(54900);preparations=bytearray();environments=[]
for i in range(2048):
    z=(0.25,1,4,16,-1,0)[i%6];center=(rng.randint(-256,256)/32,rng.randint(-256,256)/32,z)
    radius=rng.randint(1,256)/32;angle=rng.randint(-512,512)/64
    environment=struct.pack('<IIIf',1,1,i%2,8)
    preparations.extend(struct.pack('<5fII3f',*center,angle,radius,32,64,1,1,1)+environment)
    environments.append(environment)
packets=subprocess.check_output([str(c['probe']),'--particle-billboard-prepare'],input=preparations)
commands=bytearray();results=bytearray();histogram={}
for case,environment in enumerate(environments):
    packet=packets[case*112:(case+1)*112];assert packet[:4]==bytes(4);packet=packet[4:]
    command=environment+packet;commands.extend(command)
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x549270,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    u.mem_write(0x17c7bcc,struct.pack('<I',0x66));u.mem_write(0x5a4d18,b'\1\1');u.mem_write(0x1818b65,bytes([case%2]));u.mem_write(0x1818b6c,struct.pack('<f',8))
    for i in range(4):
        v=packet[i*24:(i+1)*24];address=base+0x200+i*48
        u.mem_write(address,v[:12]+bytes(12)+bytes([struct.unpack_from('<I',v,20)[0]])+bytes(3)+v[12:20]+bytes(12))
        u.mem_write(base+0x1000+i*4,struct.pack('<I',address))
    and_code,or_code=struct.unpack_from('<II',packet,100)
    u.mem_write(base+0x2000,struct.pack('<I',4)+bytes([or_code,and_code]))
    u.mem_write(stack,struct.pack('<6I',stop,base+0x1000,base+0x1100,base+0x2000,base+0x2004,1));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x549e00,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    count=struct.unpack('<I',u.mem_read(base+0x2000,4))[0];assert count<=12
    pointers=u.reg_read(UC_X86_REG_EAX);result=bytearray(struct.pack('<II',0,count))
    for i in range(count):
        address=struct.unpack('<I',u.mem_read(pointers+i*4,4))[0]
        result.extend(u.mem_read(address,12));result.extend(u.mem_read(address+28,8));result.extend(struct.pack('<I',u.mem_read(address+24,1)[0]))
    result.extend(bytes((12-count)*24));result.extend(packet[96:100]);masks=u.mem_read(base+0x2004,2);result.extend(struct.pack('<II',masks[1],masks[0]));results.extend(result)
    histogram[count]=histogram.get(count,0)+1
    x.mem_write(base,command);x.mem_write(base+0x400,bytes([0xa5])*304)
    x.mem_write(stack,struct.pack('<4I',stop,base,base+16,base+0x400));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,304))
    assert actual==result,(case,count,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['probe']),'--particle-billboard-clip'],input=commands)
assert actual==results,[(i//308,i%308,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
commands.clear();results.clear()
for offset,bits,status in ((36,1,-3),(36,64,-3),(16,0x7fc00000,-4),(28,0x7f800000,-4),(112,0x7f800000,-4),(12,0x7fc00000,-4)):
    invalid=bytearray(command);struct.pack_into('<I',invalid,offset,bits)
    result=struct.pack('<i',status)+bytes([0xa5])*304
    commands.extend(invalid);results.extend(result)
    x.mem_write(base,bytes(invalid));x.mem_write(base+0x400,bytes([0xa5])*304)
    x.mem_write(stack,struct.pack('<4I',stop,base,base+16,base+0x400));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,304))==result,offset
assert subprocess.check_output([str(c['probe']),'--particle-billboard-clip'],input=commands)==results
report=dict(result='PASS',cases=2048,invalid_guards=6,vertex_counts=histogram,
    scope='Original 549e00, 549bd0, 549310 and temporary pool executed unchanged with UV-only draw flags, on prepared convex billboards. Exact PC/NXDK ordered positions, UVs, masks and counts; screen projection and GPU drawing excluded.')
(root/'artifacts/particle-clip-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
