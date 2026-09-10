"""Original local cone sampler 4fadb0 and actual CRT RNG versus PC/NXDK C."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop,thread=(c[k] for k in ('u','x','root','base','stack','stop','thread'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_cone_sample\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
assert bytes(u.mem_read(0x5894ac,4))==struct.pack('<f',6.283185307179586)
rng=random.Random(4);commands=bytearray();results=bytearray()
for case in range(4096):
    cosine=(-1,-0.5,0,0.5,0.99999994,1)[case%6] if case<96 else rng.randint(-32768,32768)/32768
    seed=rng.getrandbits(32);command=struct.pack('<fI',cosine,seed);commands.extend(command)
    u.mem_write(thread+20,struct.pack('<I',seed));u.mem_write(base,bytes([0xa5])*12)
    u.mem_write(stack,struct.pack('<If',stop,cosine));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,base)
    u.emu_start(0x4fadb0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    next_seed=(seed*214013+2531011)&0xffffffff;next_seed=(next_seed*214013+2531011)&0xffffffff
    assert bytes(u.mem_read(thread+20,4))==struct.pack('<I',next_seed)
    result=bytes(4)+bytes(u.mem_read(thread+20,4))+bytes(u.mem_read(base,12));results.extend(result)
    x.mem_write(base,command);x.mem_write(base+16,bytes([0xa5])*12)
    x.mem_write(stack,struct.pack('<IfII',stop,cosine,base+4,base+16));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,4))+bytes(x.mem_read(base+16,12))==result,case
for cosine in (-2,2,float('nan'),float('inf')):
    command=struct.pack('<fI',cosine,seed);commands.extend(command);result=struct.pack('<iI',-4,seed)+bytes([0xa5])*12;results.extend(result)
    x.mem_write(base,command);x.mem_write(base+16,bytes([0xa5])*12)
    x.mem_write(stack,struct.pack('<IfII',stop,cosine,base+4,base+16));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+4,4))+bytes(x.mem_read(base+16,12))==result
actual=subprocess.check_output([str(c['probe']),'--particle-cone'],input=commands)
assert actual==results,[(i//20,i%20,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=4096,invalid_guards=4,scope='Original 4fadb0 with actual random helpers and x87 sqrt/sin/cos. Exact local XYZ and two-draw RNG advancement versus PC/NXDK. World rotation, full emitter emission and live effects excluded.')
(root/'artifacts/particle-cone-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
