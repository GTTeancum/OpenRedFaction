"""VFX scale/quaternion/translation stages against original math helpers."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OWNER=B+0x1000;FACE=B+0x2000;UV=B+0x3000;PTR=B+0x4000;CTX=B+0x5000;OUT=B+0x6000;STACK=B+0xe000;STOP=B+0xff00
read=lambda u,a:struct.unpack('<I',u.mem_read(a,4))[0]
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(B,65536);return u
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
o=machine(exe);x=machine(root/'build/xbox/main.exe')
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)



import ctypes as c
class Light(c.Structure):
 _fields_=[(n,c.c_uint32) for n in ('offset','bytes','uid')]+[(n,c.c_char*256) for n in ('name','script')]+[('position',c.c_float*3),('orientation_disk',c.c_float*9),('header_byte',c.c_uint32),('flags',c.c_uint32),('color',c.c_uint8*4)]+[(n,c.c_float) for n in ('radius','inner_angle','outer_delta','cone_scale')]+[('profile',c.c_uint32),('length',c.c_float),('cycle',c.c_float*6)]
rng=random.Random(0x45f740);inputs=[];responses=[];P=0xc4e7d8;draw=0;records=[r for l in json.loads((root/'artifacts/level-lights.json').read_text())['results'] for r in l['records']]
def original(address,args=(),this=0):
 o.mem_write(STACK,w(STOP,*args));o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ECX,this);o.reg_write(UC_X86_REG_FPCW,0x37f);o.emu_start(address,STOP,count=1000000);assert o.reg_read(UC_X86_REG_EIP)==STOP
def rand_hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret=read(cpu,sp);cpu.reg_write(UC_X86_REG_EAX,draw);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
o.hook_add(UC_HOOK_CODE,rand_hook,begin=0x57312d,end=0x57312d)
for i in range(len(records)+512):
 r=dict(records[i%len(records)]);synthetic=i>=len(records)
 if synthetic:r['flags']=(r['flags']&~0xf00)|((3+i%2)<<8);r['cycle']=[.8,.2,.5,.1,.4,.6]
 record=Light()
 for name,typ in Light._fields_:
  value=r[name]
  if typ==c.c_char*256:setattr(record,name,value.encode('cp1252'))
  elif issubclass(typ,c.Array):getattr(record,name)[:]=value
  else:setattr(record,name,value)
 loader_default=i%2;draw=rng.randrange(32768);data=w(loader_default,draw)+bytes(record);inputs.append(data);x.mem_write(B,data)
 status=call('rf_level_light_activate',[B+8,loader_default,draw,OUT]);assert status==0,(i,r['name'],r['flags'],status)
 assert call('rf_vfx_light_create',[OUT,OUT+128])==0;x.mem_write(OUT+128+76,bytes([read(x,OUT+84)]));got=bytes(x.mem_read(OUT+128,80));responses.append(w(0)+bytes(x.mem_read(OUT,100))+got)
 o.mem_write(OWNER,bytes(160));o.mem_write(OWNER+0x94,w(0xffffffff));o.mem_write(P,bytes(268));o.mem_write(0xc96768,w(0xc96768,0xc96768));o.mem_write(0xc4e6b8,w(0xc4e6b8,0xc4e6b8));o.mem_write(0xc96880,w(0));o.mem_write(0x879af8,b'\1')
 original(0x45fbc0,[record.flags],OWNER)
 if loader_default:o.mem_write(OWNER+0x86,b'\0')
 o.mem_write(OWNER+12,f(*record.position));orientation=list(record.orientation_disk);o.mem_write(OWNER+24,f(*(orientation[3:]+orientation[:3])))
 o.mem_write(OWNER+0x44,f(*[v*0.003921568859368563 for v in record.color[:3]]));o.mem_write(OWNER+0x50,f(record.radius));o.mem_write(OWNER+0x58,f(record.inner_angle,record.outer_delta,record.cone_scale));o.mem_write(OWNER+0x64,w(record.profile));o.mem_write(OWNER+0x68,f(record.length))
 cycle=list(record.cycle);o.mem_write(OWNER+0x6c,f(cycle[0],cycle[3],cycle[1],cycle[2],cycle[4],cycle[5]));original(0x45f740,this=OWNER)
 expected=bytearray(80);expected[:4]=o.mem_read(P+8,4);expected[4:8]=o.mem_read(P+0x54,4);expected[8:44]=o.mem_read(P+12,36);expected[44:56]=o.mem_read(P+0x40,12);expected[56:60]=o.mem_read(P+0x3c,4);expected[60:64]=o.mem_read(P+0x38,4);expected[64:72]=o.mem_read(P+0x84,8);expected[72]=o.mem_read(P+0x4e,1)[0];expected[76:78]=o.mem_read(P+0x4c,2)
 assert got==expected,(i,record.flags,got.hex(),expected.hex())
 assert read(x,OUT+88)==o.mem_read(OWNER+0x90,1)[0] and bytes(x.mem_read(OUT+92,4))==bytes(o.mem_read(OWNER+0x88,4)) and read(x,OUT+96)==read(o,P+0x50),(i,'activation state')
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-light-activate'],input=b''.join(inputs));assert pc==b''.join(responses)
report=dict(result='PASS',original_pc_nxdk_authored_records=len(records),randomized_cycle_cases=512,scope='Original45fbc0/45f740 and actual constructors/enable. Prepared authored owner fields; only15-bit RNG draw supplied for synthetic cycles. All80 candidate fields, phase/delay/visibility match original/NXDK; PC matches all activation/candidate bytes. No original file-reader execution, replacement ownership, timer stepping or native activation.')
(root/'artifacts/level-light-activation.json').write_text(json.dumps(report,indent=2));print(report)
