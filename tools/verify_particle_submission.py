"""Complete original 5587c0 particle submission vertices versus shared PC/NXDK C."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_billboard_project\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
submitted=None
def hook(m,address,size,data):
    global submitted
    sp=m.reg_read(UC_X86_REG_ESP);ret,count,pointers,flags,mode=struct.unpack('<5I',m.mem_read(sp,20))
    assert flags==1 and mode==0x118c42 and count<=12
    submitted=bytearray(struct.pack('<II',0,count))
    for i in range(count):
        address=struct.unpack('<I',m.mem_read(pointers+i*4,4))[0]
        submitted.extend(m.mem_read(address,24));submitted.extend(m.mem_read(address+28,8))
    submitted.extend(bytes((12-count)*32))
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook,begin=0x551900,end=0x551900)
rng=random.Random(5587);preparations=bytearray();environments=[];projections=[]
for i in range(2048):
    center=(rng.randint(-256,256)/32,rng.randint(-256,256)/32,(0.25,1,4,16,-1,0)[i%6])
    environment=struct.pack('<IIIf',i%3!=0,1,i%2,8)
    preparations.extend(struct.pack('<5fII3f',*center,rng.randint(-512,512)/64,rng.randint(1,256)/32,32,64,1,1,1)+environment)
    environments.append(environment)
    projections.append(struct.pack('<I3f2i',(0,1,256,257)[i%4],(0,0.1,-0.1)[i%3],320,240,-16,7))
packets=subprocess.check_output([str(c['probe']),'--particle-billboard-prepare'],input=preparations)
commands=bytearray();results=bytearray();drawn=0;counts={}
for case,(environment,projection) in enumerate(zip(environments,projections)):
    packet=packets[case*112+4:(case+1)*112];commands.extend(projection+environment+packet)
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x549270,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    enabled,depth,far,distance=struct.unpack('<IIIf',environment)
    u.mem_write(0x17c7bcc,struct.pack('<I',0x66));u.mem_write(0x5a4d18,bytes([enabled,depth]));u.mem_write(0x1818b65,bytes([far]));u.mem_write(0x1818b6c,struct.pack('<f',distance))
    u.mem_write(0x5a445a,projection[:1]);u.mem_write(0x1e652e8,projection[4:8]);u.mem_write(0x1818a5c,projection[8:12]);u.mem_write(0x1818a24,projection[12:16]);u.mem_write(0x17c7bec,projection[16:24])
    for i in range(4):
        v=packet[i*24:(i+1)*24];address=base+0x200+i*48
        u.mem_write(address,v[:12]+bytes(12)+bytes([struct.unpack_from('<I',v,20)[0]])+bytes(3)+v[12:20]+bytes(12))
        u.mem_write(base+0x1000+i*4,struct.pack('<I',address))
    override=struct.unpack_from('<f',packet,96)[0];submitted=None
    u.mem_write(stack,struct.pack('<6If',stop,4,base+0x1000,1,0x118c42,1,override))
    u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x5587c0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    result=bytes(392) if submitted is None else bytes(submitted);results.extend(result)
    count=struct.unpack_from('<I',result,4)[0];drawn+=count>0;counts[count]=counts.get(count,0)+1
    x.mem_write(base,projection+environment+packet);x.mem_write(base+0x400,bytes([0xa5])*388)
    x.mem_write(stack,struct.pack('<5I',stop,base,base+24,base+40,base+0x400));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=200000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x400,388))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['probe']),'--particle-billboard-project'],input=commands)
assert actual==results,[(i//392,i%392,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=2048,submitted=drawn,vertex_counts=counts,
    scope='Full unchanged 5587c0, actual clipping/projection/depth override and temporary cleanup, with 551900 recording submitted vertices instead of a GPU. PC/NXDK exact camera XYZ, screen XY, reciprocal depth, UVs and draw/reject decisions. Color conversion, rasterization and live scene effects excluded.')
(root/'artifacts/particle-submission-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
