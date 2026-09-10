"""Original full 4d3c40 ordinary draw queue versus shared PC/NXDK ordering."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
entry=int(re.search(r'_rf_render_sphere_order\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000)
camera=(0,0,0);drawn=[]
def hook(m,address,size,data):
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    if address==0x518a70:
        pos,basis=struct.unpack('<2I',m.mem_read(sp+4,8))
        m.mem_write(pos,struct.pack('<3f',*camera));m.mem_write(basis,struct.pack('<9f',1,0,0,0,1,0,0,0,1));pop=4
    else:
        record=m.reg_read(UC_X86_REG_ECX);assert 0x88fd20<=record<0x88fd20+2048*48
        drawn.append((record-0x88fd20)//48);pop=12
    m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x518a70,0x4d3460):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x4d3c40);commands=bytearray();results=bytearray();total=0;ties=0
for case in range(160):
    count=(0,1,2,4,7,31,128,2048)[case%8]
    camera=tuple(rng.randint(-32,32)/4 for _ in range(3));records=bytearray(count*48);entries=bytearray()
    for i in range(count):
        # Frequent equal lengths reveal the original non-stable gap exchanges.
        pos=tuple(camera[j]+rng.randint(-4,4) for j in range(3))
        if i%5==0:pos=(camera[0]+3,camera[1]+4,camera[2]);ties+=1
        radius=(-1,0,.25,3)[(i+case)%4];sorted_flag=(0,1,2,255,256,257)[(i+case)%6]
        entries.extend(struct.pack('<4fI',*pos,radius,sorted_flag))
        struct.pack_into('<I4f',records,i*48,i,*pos,radius);records[i*48+20]=sorted_flag&255
    u.mem_write(0x9bb550,struct.pack('<I',count));u.mem_write(0x9bb5ac,bytes(4))
    if records:u.mem_write(0x88fd20,bytes(records))
    u.mem_write(stack,struct.pack('<5I',stop,0,0,0,0));u.reg_write(UC_X86_REG_ESP,stack)
    drawn=[];u.emu_start(0x4d3c40,stop,count=5000000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert len(drawn)==count and sorted(drawn)==list(range(count))
    distances=b''.join(bytes(u.mem_read(0x88fd20+i*48+40,4)) for i in range(count))
    result=bytes(4)+struct.pack('<'+'I'*count,*drawn)+distances;results.extend(result)
    commands.extend(struct.pack('<I3f',count,*camera)+entries)
    x.mem_write(base,struct.pack('<3f',*camera))
    if entries:x.mem_write(base+0x10000,bytes(entries))
    x.mem_write(stack,struct.pack('<6I',stop,base+0x10000,count,base,base+0x20000,base+0x22000));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=5000000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x20000,count*4))+bytes(x.mem_read(base+0x22000,count*4))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
    total+=count
actual=subprocess.check_output([str(c['probe']),'--render-sphere-order'],input=commands)
assert actual==results,[(i,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=160,entries=total,equal_distance_inputs=ties,scope='Full unchanged 4d3c40 and actual 4d43e0 ordinary partition; resolved camera supplied and final callbacks recorded. PC/NXDK exact distance keys and complete callback order, including low-byte flags, negative radii, ties and 2048-entry limit. No plane associations, room-plane split, culling, GPU or live scene integration.')
(root/'artifacts/render-sphere-order-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
