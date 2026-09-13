"""Original corona camera vector/math callees versus PC and compiled NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ECX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+16;OUT=B+0x1000;CAM=B+0x2000;BASIS=CAM+16
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_corona_camera_setup\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
u.mem_write(CB,b'\xdd\x1d'+w(OUT+24)+b'\xc3')
identity=[1,0,0,0,1,0,0,0,1];rng=random.Random(414860);cases=[]
for n in range(512):
 a=rng.uniform(-math.pi,math.pi);b=rng.uniform(-math.pi,math.pi)
 axis=[math.cos(a),0,math.sin(a),0,1,0,-math.sin(a),0,math.cos(a)]
 basis=[math.cos(b),0,math.sin(b),0,1,0,-math.sin(b),0,math.cos(b)]
 cases.append(f(*[rng.uniform(-100,100) for _ in range(3)],*axis,*[rng.uniform(-100,100) for _ in range(3)],*basis))
for position in ((0,0,1),(0,0,-1),(1,0,0),(0,1,0),(1,1,1)):
 cases.append(f(*position,*identity,0,0,0,*identity))
base=len(cases);cases += [f(0,0,0,*identity,0,0,0,*identity),f(0,0,1,*identity,0,0,0,*([0]*6+[0,0,2]))]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--corona-camera'],input=b''.join(cases));assert len(actual)==36*len(cases)
maximum=0;exact=0
for n,wire in enumerate(cases):
 if n<base:
  u.mem_write(B,bytes(768));u.mem_write(B+0x3c,wire[:12]);u.mem_write(B+0x48,wire[12:48]);u.mem_write(S,bytes(512));u.mem_write(S+0x30,wire[48:60]);u.mem_write(S+0x60,wire[60:96]);u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x4148f1,0x414948,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x414948
  u.emu_start(0x414a73,0x414aa8,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x414aa8
  u.mem_write(S,w(STOP));u.emu_start(CB,STOP,count=100);angle=struct.unpack('<d',u.mem_read(OUT+24,8))[0]
  direction=bytes(u.mem_read(S+0x3c,12));glare=bytes(u.mem_read(S+0x28,4));distance=bytes(u.mem_read(S+0x24,4))
  u.mem_write(S-16,w(STOP,S+0x3c));u.reg_write(UC_X86_REG_ESP,S-16);u.reg_write(UC_X86_REG_ECX,S+0x60);u.emu_start(0x40a0b0,STOP,count=100)
  u.mem_write(S,w(STOP));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(CB,STOP,count=100);side=struct.unpack('<d',u.mem_read(OUT+24,8))[0]
  expected=w(0)+direction+glare+distance+f(-1 if side<0 else 1 if side>0 else 0)
 else:expected=w(-2)+b'\xa5'*24;angle=None
 x.mem_write(B,bytes(528));x.mem_write(B+152,wire[:12]);x.mem_write(B+164,wire[12:48]);x.mem_write(CAM,wire[48:60]);x.mem_write(BASIS,wire[60:96]);x.mem_write(OUT,b'\xa5'*32)
 x.mem_write(S,w(STOP,B,CAM,BASIS,OUT,OUT+24));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=100000)
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(OUT,32));pc=actual[n*36:(n+1)*36]
 assert got[:28]==expected and pc[:28]==expected,(n,got.hex(),pc.hex(),expected.hex())
 if angle is None:assert got[28:]==b'\xa5'*8 and pc[28:]==b'\xa5'*8
 else:
  for raw in (got[28:],pc[28:]):
   value=struct.unpack('<d',raw)[0];error=abs(value-angle);maximum=max(maximum,error);exact+=value==angle
   assert error<=1e-14,(n,value,angle)
report={'result':'PASS','original_cases':base,'guard_cases':2,'angle_comparisons':base*2,'bit_exact_angles':exact,'max_angle_error_radians':maximum,'original_sha256':sha,'scope':'Original4148f1..414948 and414a73..414aa8 actual vector subtraction, normalization, distance, dot and CRT acos execute without hooks. Direction/float degree/distance/side sign exact PC/NXDK; double acos tolerance1e-14. No parent gates, native camera binding or draw claim.'}
(root/'artifacts/analysis/corona-camera.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
