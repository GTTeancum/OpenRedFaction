"""Original 4d3560 append with actual 5186a0 culling versus shared PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_render_queue_append\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x20000)
rng=random.Random(0x4d3560);commands=bytearray();results=bytearray();appended=accepted_only=rejected=0
original_records=0x88fd20;initial=bytes([0xa5])*(2048*48)
u.mem_write(0x88fd1c,b'\0');u.mem_write(0x9bb56c,bytes(4));u.mem_write(0x87bb00,bytes(12))
for case in range(256):
    count=(0,1,2047,2048)[case%4];planes=(case//4)%7
    position=tuple(rng.randint(-32,32)/4 for _ in range(3));radius=(-1,0,.25,2,8)[case%5]
    frustum=bytearray(156)
    for i in range(6):
        normal=[0.,0.,0.];normal[i//2]=(-1.,1.)[i%2]
        distance=-(normal[0]*position[0]+normal[1]*position[1]+normal[2]*position[2])+radius
        # Tangency and one representable displacement to either side.
        distance+=(-.125,0,.125)[(case+i)%3]
        plane=struct.pack('<4fI',*normal,distance,0x1234)
        frustum[i*20:(i+1)*20]=plane;u.mem_write(0x1818a6c+i*28,plane+bytes(8))
    struct.pack_into('<I',frustum,144,planes);u.mem_write(0x1818b8c,struct.pack('<I',planes))
    record=bytearray(rng.randbytes(48));struct.pack_into('<4f',record,4,*position,radius)
    callback=0 if case%3==0 else 0x12345678;struct.pack_into('<I',record,44,callback)
    command=bytes(frustum)+bytes(record)+struct.pack('<I',count);commands.extend(command)
    u.mem_write(original_records,initial);u.mem_write(0x9bb550,struct.pack('<I',count));u.mem_write(0x9bb568,bytes(4))
    u.mem_write(base+0x200,struct.pack('<3f',*position))
    obj=struct.unpack_from('<I',record)[0];plane,minimum,maximum=struct.unpack_from('<3I',record,28)
    u.mem_write(stack,struct.pack('<IIIIf7I',stop,obj,base+0x200,base+0x200,radius,callback,record[20],plane,minimum,maximum,record[23],record[24]))
    u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4d3560,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    accepted=u.reg_read(UC_X86_REG_EAX)&255;after=struct.unpack('<I',u.mem_read(0x9bb550,4))[0]
    result=struct.pack('<III',0,accepted,after)+bytes(u.mem_read(original_records,len(initial)));results.extend(result)
    appended+=after>count;accepted_only+=bool(accepted and after==count);rejected+=not accepted
    x.mem_write(base,command);x.mem_write(base+0x10000,initial);x.mem_write(base+0x300,struct.pack('<II',count,0))
    x.mem_write(stack,struct.pack('<8I',stop,base,base+160,base+156,base+0x10000,2048,base+0x300,base+0x304));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x304,4))+bytes(x.mem_read(base+0x300,4))+bytes(x.mem_read(base+0x10000,len(initial)))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:15])
actual=subprocess.check_output([str(c['probe']),'--render-queue-append'],input=commands)
assert actual==results,[(i,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:15]
assert appended and accepted_only and rejected
report=dict(result='PASS',cases=256,appended=appended,accepted_without_callback=accepted_only,rejected=rejected,scope='Full unchanged original 4d3560, actual vector helpers and 5186a0/4163a0 plane rejection, zero resolved world offset/no instance. PC/NXDK exact acceptance, count and all 2048 slot bytes. Tangency, signed radius, null callbacks, full queue and stale-slot preservation. Camera plane construction, emitter collection and GPU dispatch not part of this replay.')
(root/'artifacts/render-queue-append-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
