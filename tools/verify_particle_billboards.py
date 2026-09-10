"""Original camera-space billboard corner and UV construction vs PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u=c['u'];x=c['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EAX
width=height=1

def hook(m,a,size,data):
 if a==0x555483:m.emu_stop();return
 sp=m.reg_read(UC_X86_REG_ESP);ret,bitmap,wp,hp=struct.unpack('<4I',m.mem_read(sp,16));m.mem_write(wp,struct.pack('<I',width));m.mem_write(hp,struct.pack('<I',height));m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook,begin=0x510630,end=0x510630);u.hook_add(UC_HOOK_CODE,hook,begin=0x555483,end=0x555483)
entry=int(re.search(r'_rf_particle_billboard_build\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();expected=bytearray();rng=random.Random(555230)
for case in range(1024):
 width,height=((32,32),(64,32),(32,64),(256,16),(16,256),(127,63),(63,127),(1,1))[case%8]
 center=tuple(rng.randint(-1000,1000)/16 for _ in range(3));angle=(case%65-32)/8;radius=(case%17)/4;scale=((case%7+1)/4,(case%11+1)/4)
 command=struct.pack('<5fII2f',*center,angle,radius,width,height,*scale);commands.extend(command)
 u.mem_write(base,struct.pack('<3f',*center));u.mem_write(stack+0xf0,struct.pack('<ff',angle,radius));u.mem_write(0x17c7c18,bytes(4));u.mem_write(0x1818b48,struct.pack('<2f',*scale));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.emu_start(0x5552a0,0x555483,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x555483
 sp=u.reg_read(UC_X86_REG_ESP);result=bytearray(4)
 for slot in (3,2,1,0):
  address=sp+0x34+slot*48;result.extend(u.mem_read(address,12));result.extend(u.mem_read(address+0x1c,8))
 expected.extend(result)
 x.mem_write(base,command);x.mem_write(base+0x100,bytes([0xa5])*80);x.mem_write(stack,struct.pack('<IIffIIII',stop,base,angle,radius,width,height,base+28,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,80));assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b])
actual=subprocess.check_output([str(c['probe']),'--particle-billboard'],input=commands);assert actual==expected,[(i//84,i%84,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',cases=1024,x87_control_word='0x027f',scope='Original 5552a0..555483, bitmap dimensions supplied, actual fsin/fcos and aspect/corner math. Exact PC/NXDK corners/UVs in submission order. Projection/clipping/depth bias/blending/drawing excluded.')
(root/'artifacts/particle-billboard-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
