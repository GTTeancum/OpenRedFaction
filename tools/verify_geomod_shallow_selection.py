"""Execute original45cff0 shallow selection with actual membership and arithmetic; container only supplied, no old cuts."""
import sys,struct,json,hashlib,math
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import *
from unicorn.x86_const import *
im=pefile.PE(str(R/'Installed_Game/RF.exe')).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);B=0x30000000;u.mem_map(B,0x100000);S=B+0xe0000;stop=B+0xf0000;w=lambda *v:struct.pack('<'+'I'*len(v),*v);f=lambda *v:struct.pack('<'+'f'*len(v),*v);rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0];regions=[]
def hook(cpu,a,size,_):
 if a not in [0x40a490,0x40a480]:return
 assert cpu.reg_read(UC_X86_REG_ECX)==0x6460a4
 sp=cpu.reg_read(UC_X86_REG_ESP);v=len(regions) if a==0x40a490 else B+0x2000+rd(sp+4)*4;cpu.reg_write(UC_X86_REG_EAX,v);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+(4 if a==0x40a490 else 8))
u.hook_add(UC_HOOK_CODE,hook);u.mem_write(0x1754474,w(7));u.mem_write(0x647c9c,w(0));u.mem_write(0x646004,w(55));rows=[]
configs=[[(0,1,0)],[(0,1,0),(0,1,0)],[(0,1,0),(1,0,0)],[(0,1,0),(0,-1,0)],[(0,1,0),(1,0,0),(0,0,1)],[(0,1,0),(1,0,0),(0,1,0)]]
for dot in [.9500000476837158,.949999988079071,.9499999284744263,-.09999999403953552,-.10000000149011612,-.10000000894069672]:configs.append([(0,1,0),(math.sqrt(1-dot*dot),dot,0)])
for normals in configs:
 for hardness in [25,100]:
  regions=normals;u.mem_write(B,bytes(0x8000));u.mem_write(B+0x103c,f(5));
  for i,n in enumerate(normals):
   a=B+0x3000+i*0x50;u.mem_write(B+0x2000+i*4,w(a));u.mem_write(a,w(2,hardness)+bytes([1,0,0,0])+f(2+i,0,0,0,1,0,0,*n,0,0,1,100,100,100,100))
  u.mem_write(S,w(stop,B,B+0x1000,1));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x45cff0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
  accepted=u.reg_read(UC_X86_REG_EAX)&255;limits=struct.unpack('<6f',u.mem_read(B+0x104c,24));scale=struct.unpack('<f',u.mem_read(B+0x103c,4))[0]
  selected=[];valid=True
  for i,n in enumerate(normals):
   if len(selected)==1 and n[1]>.949999988079071:continue
   selected.append((i,n))
   if len(selected)>2 or (len(selected)==2 and n[1]<-.10000000149011612):valid=False;break
  expected=int(valid and hardness!=100);assert accepted==expected,(normals,hardness,accepted)
  if accepted:
   values=[]
   for i,n in selected:values.extend([-x*(2+i) for x in n])
   values+=([0.0]*(6-len(values)));assert f(*limits)==f(*values) or all(abs(a-b)<1e-6 for a,b in zip(limits,values));assert scale==3.75
  rows.append(dict(normals=normals,hardness=hardness,accepted=accepted,limits=limits,scale=scale))
(R/'artifacts/crater-shading-re/shallow-selection.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest(),scope=__doc__,rows=rows),indent=2));print('PASS:24 original shallow selection/depth/hardness cases')
