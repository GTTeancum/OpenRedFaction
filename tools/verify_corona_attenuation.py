"""Original414a73..414c12 attenuation after resolved distance/acos, PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+16;ENV=B+0x1000;DEF=B+0x2000;OUT=B+0x3000;DIST=OUT+32;ANGLE=DIST+8
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_corona_attenuate\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(CB,b'\xdd\x05'+w(DIST)+b'\xc3');u.mem_write(CB+16,b'\xdd\xd8\xdd\x05'+w(ANGLE)+b'\xc3')
def hook(cpu,address,size,context):
 if address==0x4faed0:cpu.reg_write(UC_X86_REG_EIP,CB)
 elif address==0x573630:cpu.reg_write(UC_X86_REG_EIP,CB+16)
u.hook_add(UC_HOOK_CODE,hook)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
rng=random.Random(41473);cases=[]
for n in range(512):
 env=[rng.uniform(.1,100),rng.uniform(0,180),rng.uniform(40,120),rng.uniform(.1,1),rng.uniform(.1,1)]
 definition=[rng.uniform(1,90),rng.uniform(0,3),rng.uniform(.1,2),rng.uniform(.1,4),rng.uniform(0,3)]
 angle=rng.uniform(0,math.pi);samples=[rng.random(),rng.random(),rng.uniform(0,5),rng.uniform(0,5)]
 cases.append([*map(f,env),*struct.unpack('<II',struct.pack('<d',angle)),*map(f,definition),*map(f,samples),n%2])
for distance in (.1,12,40):
 for glare_angle in (0,30,40,50):
  for view in (0,1):cases.append([*map(f,[distance,glare_angle,90,.5,.5]),0,0,*map(f,[30,1,1,1,0]),*map(f,[0,.2,0,1]),view])
base=len(cases)
for bad in ('distance','view'):
 c=cases[0].copy();c[0 if bad=='distance' else 16]=0 if bad=='distance' else 2;cases.append(c)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--corona-attenuation'],input=b''.join(w(*c) for c in cases));assert len(actual)==20*len(cases)
for n,c in enumerate(cases):
 distance,glare_angle,fov,intensity_scale,size_scale=c[:5];view=c[16]
 if n<base:
  u.mem_write(B,bytes(768));u.mem_write(B+0x29c,w(*c[12:16]));u.mem_write(DEF,bytes(128));u.mem_write(DEF+16,w(*c[7:12]));u.mem_write(S,bytes(512));u.mem_write(S+0x28,w(glare_angle));u.mem_write(S+0x114,w(view))
  u.mem_write(0x59613c,w(fov));u.mem_write(0x5943fc,w(intensity_scale));u.mem_write(0x594400,w(size_scale));u.mem_write(DIST,struct.pack('<d',struct.unpack('<f',w(distance))[0]));u.mem_write(ANGLE,w(*c[5:7]))
  u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EDI,DEF);u.reg_write(UC_X86_REG_EBP,B+0x3c);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x414a73,0x414c12,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==0x414c12
  want=w(0,r(u,S+0x14),r(u,S+0x20),r(u,S+0x18),r(u,S+0x1c))
 else:want=w(-2 if n==base else -4)+b'\xa5'*16
 x.mem_write(B,bytes(104));x.mem_write(B+24,w(*c[12:16]));x.mem_write(DEF,bytes(320));x.mem_write(DEF+268,w(*c[7:12]));x.mem_write(ENV,w(*c[:5]));x.mem_write(OUT,b'\xa5'*16);x.mem_write(S,w(STOP,B,view,DEF,ENV,*c[5:7],OUT));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=100000)
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,16))
 assert got==want and actual[n*20:(n+1)*20]==want,(n,c,got.hex(),want.hex(),actual[n*20:(n+1)*20].hex())
report={'result':'PASS','original_cases':base,'guard_cases':2,'original_sha256':sha,'scope':'Original414a73..414c12 with resolved distance/acos supplied; actual dot/clamp/max/sqrt and x87 arithmetic execute. Exact intensity, size, angular and squared flash versus PC/NXDK. No upstream vector/acos, screen-flash submission or draw binding claim.'}
(root/'artifacts/analysis/corona-attenuation.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
