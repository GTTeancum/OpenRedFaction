"""Compare full original48bbe0 plane production against PC and NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
b=0x30000000;state=b+0x8000;output=b+0x9000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_projectile_planes\s+([0-9a-fA-F]+)',mapping)[1],16)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=20000)
 assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_FPCW)==0x27f
rng=random.Random(0x48bbe0);commands=[];answers=[]
for case in range(8192):
 position=[rng.randrange(-8192,8193)/8 for _ in range(3)]
 angle=rng.randrange(-3141,3142)/1000;c=math.cos(angle);s=math.sin(angle)
 basis=[c,0,s,0,1,0,-s,0,c]
 if case%3==0:basis=[rng.randrange(-256,257)/128 for _ in range(9)]
 speed=rng.choice((.125,1,10,30,100,1000,-1,-100))
 payload=f(position+basis+[speed]);commands.append(payload)
 u.mem_write(b+0xe4,payload[:12]);u.mem_write(b+0xfc,payload[12:48]);u.mem_write(b+0x294,w(b+0x4000));u.mem_write(b+0x40c0,payload[48:])
 before=bytes(u.mem_read(b,0x5000));u.mem_write(0x75db38,b'\xa5'*64)
 run(u,0x48bbe0,[b]);expected=bytes(u.mem_read(0x75db38,64));assert bytes(u.mem_read(b,0x5000))==before
 x.mem_write(state,payload);x.mem_write(output,b'\xa5'*64);run(x,entry,[state,output]);actual=bytes(x.mem_read(output,64))
 assert actual==expected,(case,struct.unpack('<16f',expected),struct.unpack('<16f',actual))
 assert bytes(x.mem_read(state,52))==payload
 answers.append(expected)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--projectile-planes'],input=b''.join(commands))

if actual!=b''.join(answers):
 i=next(i//64 for i,(a,bv) in enumerate(zip(actual,b''.join(answers))) if a!=bv)
 raise AssertionError((i,struct.unpack('<13f',commands[i]),struct.unpack('<16f',answers[i]),struct.unpack('<16f',actual[i*64:(i+1)*64])))
report=dict(result='PASS',cases=8192,original_sha256=sha,x87_control='0x027f',scope='Full original48bbe0 with all vector, normalization and plane helpers executing, no substituted callees. Exact four planes and unchanged input versus PC/NXDK; rotated orthonormal and arbitrary finite bases, positive/negative nonzero speeds. No live projectile ownership or native XEMU activation.')
(root/'artifacts/projectile-planes.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
