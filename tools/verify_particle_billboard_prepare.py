"""Original full billboard preparation through clip classification versus PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX
width=height=1
end=0x5554c9
# Only bitmap dimension lookup is supplied. Constructors, trigonometry,
# aspect math, depth helper and clip classification execute original code.
def hook(m,address,size,data):
    if address==end:m.emu_stop();return
    sp=m.reg_read(UC_X86_REG_ESP)
    ret,bitmap,wp,hp=struct.unpack('<4I',m.mem_read(sp,16))
    m.mem_write(wp,struct.pack('<I',width));m.mem_write(hp,struct.pack('<I',height))
    m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook,begin=0x510630,end=0x510630)
u.hook_add(UC_HOOK_CODE,hook,begin=end,end=end)
entry=int(re.search(r'_rf_particle_billboard_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(5554);commands=bytearray();results=bytearray();culled=partial=inside=0
for case in range(2048):
    width,height=((32,32),(64,32),(32,64),(127,63))[case%4]
    radius=(case%17)/4
    z=(0,-1,1,2,10,100,radius,radius+2**-20)[case%8]
    center=(rng.randint(-1000,1000)/32,rng.randint(-1000,1000)/32,z)
    angle=(case%65-32)/8
    scale=((case%7+1)/4,(case%11+1)/4,(-1,0,0.5,1,2,3)[case%6])
    enabled=(0,1,256,257)[case%4];depth=(0,1,256,257)[(case//4)%4];far=(0,1,256,257)[(case//16)%4]
    distance=(0,1,10,100)[(case//64)%4]
    command=struct.pack('<5fII3fIIIf',*center,angle,radius,width,height,*scale,enabled,depth,far,distance)
    commands.extend(command)
    u.mem_write(base,struct.pack('<3f',*center)+bytes(13)+b'\1'+bytes(22))
    u.mem_write(0x17c7c18,bytes(4));u.mem_write(0x17c7bcc,struct.pack('<I',0x66))
    u.mem_write(0x1818b48,struct.pack('<3f',*scale))
    u.mem_write(0x5a4d18,bytes([enabled&255,depth&255]));u.mem_write(0x1818b65,bytes([far&255]));u.mem_write(0x1818b6c,struct.pack('<f',distance))
    u.mem_write(stack,struct.pack('<IIffI',stop,base,angle,radius,0x118c42));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x555230,end,count=10000);assert u.reg_read(UC_X86_REG_EIP)==end
    sp=u.reg_read(UC_X86_REG_ESP);result=bytearray(4);codes=[]
    for slot in (3,2,1,0):
        address=sp+0x30+slot*48;code=u.mem_read(address+24,1)[0];codes.append(code)
        result.extend(u.mem_read(address,12));result.extend(u.mem_read(address+28,8));result.extend(struct.pack('<I',code))
    and_code=u.reg_read(UC_X86_REG_EBX)&255;or_code=0
    for code in codes:or_code|=code
    result.extend(u.mem_read(sp+12,4));result.extend(struct.pack('<II',and_code,or_code));results.extend(result)
    if and_code:culled+=1
    elif or_code:partial+=1
    else:inside+=1
    x.mem_write(base,command);x.mem_write(base+0x100,bytes([0xa5])*108)
    x.mem_write(stack,struct.pack('<IIffIIIII',stop,base,angle,radius,width,height,base+28,base+40,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,108))
    assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b])
assert subprocess.check_output([str(c['probe']),'--particle-billboard-prepare'],input=commands)==results
commands.clear();results.clear()
for offset,bits in ((0,0x7fc00000),(12,0x7f800000),(16,0xbf800000),(20,0),(24,0),(28,0x7fc00000),(36,0x7f800000),(52,0x7fc00000)):
    invalid=bytearray(command);struct.pack_into('<I',invalid,offset,bits)
    commands.extend(invalid);result=struct.pack('<i',-4)+bytes([0xa5])*108;results.extend(result)
    x.mem_write(base,bytes(invalid));x.mem_write(base+0x100,bytes([0xa5])*108)
    a,r=struct.unpack_from('<ff',invalid,12);w,h=struct.unpack_from('<II',invalid,20)
    x.mem_write(stack,struct.pack('<IIffIIIII',stop,base,a,r,w,h,base+28,base+40,base+0x100));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,108))==result,offset
assert subprocess.check_output([str(c['probe']),'--particle-billboard-prepare'],input=commands)==results
report=dict(result='PASS',cases=2048,invalid_guards=8,culled=culled,partial=partial,inside=inside,
    scope='Original 555230..5554c9 with bitmap dimensions supplied and center already projected. Original constructors, geometry, 518660 depth bias and 5475d0 clip classification versus exact PC/NXDK packets. Polygon clipping, screen projection and GPU submission excluded.')
(root/'artifacts/particle-billboard-prepare-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
