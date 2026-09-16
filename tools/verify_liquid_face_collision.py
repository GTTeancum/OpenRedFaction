"""Original liquid-contact dispatcher numerical evidence; no native game launch."""
import sys,struct,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
exe=R/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image()
B=0x30000000;S=B+0xe0000;stop=B+0xf0000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
rows=[]
for radius in [0,.1,.125]:
 for label,y,dy in [('down',1,-2),('up',-1,2),('under_down',-.01,-2),('on_down',0,-2),('on_up',0,2),('endpoint',1,-1),('sphere_endpoint',1,-.9),('parallel',1,0),('overlap',.05,-2),('binary_endpoint',1,-.875)]:
  u.mem_write(B,bytes(0x10000)); face=B;query=B+0x1000;out=B+0x2000
  u.mem_write(face,f(0,1,0,0,-10,0,-10,10,0,10)+w(4));u.mem_write(face+0x30,w(0xffffffff));u.mem_write(face+0x40,w(B+0x3000))
  pts=[(-10,0,-10),(-10,0,10),(10,0,10),(10,0,-10)]
  for n,p in enumerate(pts):
   corner=B+0x3000+n*0x20;v=B+0x4000+n*0x20
   u.mem_write(v,f(*p));u.mem_write(corner,w(v));u.mem_write(corner+0x14,w(B+0x3000+((n+1)%4)*0x20,B+0x3000+((n-1)%4)*0x20))
  u.mem_write(query+0x4c,f(radius)+w(0x1000)+f(0,y,0,0,dy,0));u.mem_write(out,w(0)+f(1));u.mem_write(0xca06b0,w(15))
  u.mem_write(S,w(stop,face,query,out));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x4dec10,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop
  rows.append(dict(radius=radius,label=label,start_y=y,delta_y=dy,hit=rd(out),fraction=struct.unpack('<f',u.mem_read(out+4,4))[0],position=struct.unpack('<3f',u.mem_read(out+8,12)),normal=struct.unpack('<3f',u.mem_read(out+20,12)),face=hex(rd(out+36))))
assert all(r['hit']==0 for r in rows if r['label'] in ['up','under_down','on_up','parallel'])
assert all(r['hit']==1 and r['fraction']==0 for r in rows if r['label']=='on_down')
assert next(r for r in rows if r['radius']==.125 and r['label']=='binary_endpoint')['fraction']==1
assert next(r for r in rows if r['radius']==.125 and r['label']=='binary_endpoint')['hit']==1
print('PASS',len(rows),'whole unhooked original face collision cases');(R/'artifacts/crater-shading-re/liquid-face-collision.json').write_text(json.dumps(rows,indent=2))


import subprocess
commands=[]
for row in rows:
 verts=[-10,0,-10,-10,0,10,10,0,10,10,0,-10]+[0]*12
 start=[0,row['start_y'],0];delta=[0,row['delta_y'],0]
 wire=struct.pack('<41fIIiIIII',0,1,0,0,-10,0,-10,10,0,10,*verts,*start,*delta,1,0x1000,4,-1,0,0,0,4)
 commands.append(wire+struct.pack('<4f',*delta,row['radius']))
output=subprocess.check_output([str(R/'build/pc/Release/rf_collision_probe.exe'),'--sweep'],input=b''.join(commands))
assert len(output)==44*len(rows)
for i,row in enumerate(rows):
 status,hit=struct.unpack_from('<iI',output,i*44);assert status==0,(i,status)
 assert bool(hit)==bool(row['hit']),(row,hit)
 if hit:
  actual=output[i*44+8:i*44+36]
  expected=struct.pack('<7f',row['fraction'],*row['position'],*row['normal'])
  assert actual==expected,(row,actual.hex(),expected.hex())
print('PASS',len(rows),'original liquid-face cases compared with shared C sweep')

import re
xp=pefile.PE(str(R/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(B,0x100000)
entry=int(re.search(r'_rf_collision_sweep_face\s+([0-9a-fA-F]+)',(R/'build/xbox/main.map').read_text())[1],16)
for i,wire in enumerate(commands):
 face=B+4096;out=B+8192;x.mem_write(B,wire)
 x.mem_write(face,wire[:40]+struct.pack('<II',B+40,4)+wire[164:188]);x.mem_write(out,bytes([0xa5])*40)
 x.mem_write(S,struct.pack('<9I',stop,face,B+136,B+148,B+192,struct.unpack_from('<I',wire,204)[0],struct.unpack_from('<I',wire,160)[0],out,out+36))
 x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+36,4))+bytes(x.mem_read(out,36))
 assert got==output[i*44:(i+1)*44],('NXDK',i,got.hex(),output[i*44:(i+1)*44].hex())
print('PASS',len(rows),'linked NXDK liquid-face comparisons')
