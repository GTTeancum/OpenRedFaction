"""Original full 4d3c40 room-partitioned draw queue versus shared PC/NXDK ordering."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX
entry=int(re.search(r'_rf_render_room_order\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000)
u.mem_map(base+0x10000,0x10000)
camera=(0,0,0);drawn=[];before=None
def hook(m,address,size,data):
    global before
    sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
    if address==0x518a70:
        pos,basis=struct.unpack('<2I',m.mem_read(sp+4,8))
        m.mem_write(pos,struct.pack('<3f',*camera));m.mem_write(basis,struct.pack('<9f',1,0,0,0,1,0,0,0,1));pop=4
    elif address==0x4d33d0:pop=4
    elif address==0x516d40:before=len(drawn);pop=4
    else:
        record=m.reg_read(UC_X86_REG_ECX);assert 0x88fd20<=record<0x88fd20+2048*48
        drawn.append((record-0x88fd20)//48);pop=12
    m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,ret)
for address in (0x518a70,0x4d3460,0x4d33d0,0x516d40):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
rng=random.Random(0x4d3c40);commands=bytearray();results=bytearray();total=0;ties=0
for case in range(80):
    count=(0,1,2,4,7,31,128,2048)[case%8]
    camera=tuple(rng.randint(-32,32)/4 for _ in range(3));records=bytearray(count*48);entries=bytearray()
    enabled=(0,1,256,257)[(case//8)%4]
    base_y=camera[1]-5;height=5+(case%3-1)*.25
    split=struct.pack('<I2f',enabled,base_y,height)
    u.mem_write(base+0x900,bytes(0x200));u.mem_write(base+0x900+12,struct.pack('<f',base_y))
    u.mem_write(base+0x900+0x184,bytes([enabled&255]));u.mem_write(base+0x900+0x188,struct.pack('<f',height))
    u.mem_write(0x9bb590,bytes(4))
    for i in range(count):
        # Frequent equal lengths reveal the original non-stable gap exchanges.
        pos=tuple(camera[j]+rng.randint(-4,4) for j in range(3))
        if i%5==0:pos=(camera[0]+3,camera[1]+4,camera[2]);ties+=1
        radius=(-1,0,.25,3)[(i+case)%4];sorted_flag=(0,1,2,255,256,257)[(i+case)%6]
        has_plane=int(i<128 and ((i+case)%3==0 or case%16==7))
        if case%16==7:sorted_flag=1
        plane=(float((i%3)==0),float((i%3)==1),float((i%3)==2),float((i%7)-3))
        if case%4==1:plane=(.125,-.75,.3125,float((i%7)-3))
        if case%4==2:plane=(1.401298464324817e-45,0,0,0)
        entries.extend(struct.pack('<4fII4f',*pos,radius,sorted_flag,has_plane,*plane))
        bounds=(i+case)%4;minimum=pos[1]-2;maximum=pos[1]+2
        entries.extend(struct.pack('<I2f',bounds,minimum,maximum))
        for flag,offset,y in ((1,32,minimum),(2,36,maximum)):
            if bounds&flag:
                address=base+0x10000+i*24+(0 if flag==1 else 12)
                u.mem_write(address,struct.pack('<3f',pos[0],y,pos[2]));struct.pack_into('<I',records,i*48+offset,address)
        if has_plane:
            plane_address=base+0x1000+i*16
            u.mem_write(plane_address,struct.pack('<4f',*plane));struct.pack_into('<I',records,i*48+28,plane_address)
        struct.pack_into('<I4f',records,i*48,i,*pos,radius);records[i*48+20]=sorted_flag&255
    u.mem_write(0x9bb550,struct.pack('<I',count));u.mem_write(0x9bb5ac,bytes(4))
    if records:u.mem_write(0x88fd20,bytes(records))
    u.mem_write(stack,struct.pack('<5I',stop,base+0x900,0,1,0));u.reg_write(UC_X86_REG_ESP,stack)
    drawn=[];before=None;u.emu_start(0x4d3c40,stop,count=100000000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert len(drawn)==count and sorted(drawn)==list(range(count))
    distances=b''.join(bytes(u.mem_read(0x88fd20+i*48+40,4)) for i in range(count))
    assert before is not None
    result=struct.pack('<II',0,before)+struct.pack('<'+'I'*count,*drawn)+distances;results.extend(result)
    commands.extend(struct.pack('<I3f',count,*camera)+split+entries)
    x.mem_write(base,struct.pack('<3f',*camera));x.mem_write(base+0x20,split)
    if entries:x.mem_write(base+0x10000,bytes(entries))
    x.mem_write(stack,struct.pack('<9I',stop,base+0x10000,count,base,base+0x20,base+0x30000,base+0x32000,base+0x34000,base+0x200));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,4))+bytes(x.mem_read(base+0x30000,count*4))+bytes(x.mem_read(base+0x32000,count*4))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
    total+=count
actual=subprocess.check_output([str(c['probe']),'--render-room-order'],input=commands)
assert actual==results,[(i,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:20]
report=dict(result='PASS',cases=80,entries=total,equal_distance_inputs=ties,scope='Full unchanged 4d3c40 and actual 4d43e0 plane grouping and 4d4570/4163a0/40a0b0 side tests; actual 4ce080 room-height classification; resolved camera supplied, graphics-state reset omitted and final callbacks/surface-pass boundary recorded. PC/NXDK exact distance keys and complete callback order, including low-byte flags, negative radii, ties and 2048-entry limit. Includes 128 plane groups, oblique/subnormal plane coefficients, overlapping plane membership and duplicate suppression; room-height splitting with sphere/explicit bounds on both camera sides; no culling, GPU or live scene integration.')
(root/'artifacts/render-room-order-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
