"""Complete4e2650 with actual corner copying/ring moves/interpolation and validation; allocation/incidence containers supplied."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
nextface=B+0x5000;nextcorner=B+0x6000
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
def boundary(cpu,a,size,_):
 global nextface,nextcorner
 if a not in [0x4cfab0,0x4dfce0,0x4ce1e0,0x4bf550]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);pop=4;value=0
 if a==0x4cfab0:
  value=nextface;nextface+=0x100;cpu.mem_write(value,bytes(0x100));cpu.mem_write(value+0x28,bytes(cpu.mem_read(rd(sp+4),24)))
 elif a==0x4dfce0:value=nextcorner;nextcorner+=32;cpu.mem_write(value,bytes(32));pop=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,boundary)
def ring(face):
 first=rd(face+0x40);p=first;out=[]
 for _ in range(20):
  if not p:return out
  out.append(dict(corner=p,vertex=rd(p),attributes=struct.unpack('<4f',u.mem_read(p+4,16))))
  nxt=rd(p+0x14);assert rd(nxt+0x18)==p;p=nxt
  if p==first:return out
 raise AssertionError('unclosed ring')
rows=[]
for middle in [False,True]:
 u.mem_write(B,bytes(0x10000));nextface=B+0x5000;nextcorner=B+0x6000;u.mem_write(B+0x100,f(0,0,1,0));u.mem_write(B+0x128,w(0x100,0,101,0xffff0000,0xffffffff,0));u.mem_write(B+0x140,w(B+0x200))
 for i,xyz in enumerate([(-1,-1,0),(1,-1,0),(1,1,0),(-1,1,0),(0,0,0)]):u.mem_write(B+0x400+i*64,f(*xyz))
 for i in range(4):u.mem_write(B+0x200+i*32,w(B+0x400+i*64)+f(i,10+i,20+i,30+i)+w(B+0x200+(i+1)%4*32,B+0x200+(i-1)%4*32))
 vertices=[0,4,2] if middle else [0,2]
 for i,v in enumerate(vertices):u.mem_write(B+0x800+i*24,w(B+0x400+v*64)+bytes(16)+f(i));u.mem_write(B+0x1000+i*4,w(B+0x800+i*24))
 u.mem_write(S,w(stop,B+0x2000,0,len(vertices)-1,B+0x1000,B+0x3000,B+0x3004));u.reg_write(UC_X86_REG_ECX,B+0x100);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4e2650,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)&255==1
 outputs=[ring(rd(B+0x3000)),ring(rd(B+0x3004))];assert all(len(r)==(4 if middle else 3) for r in outputs)
 if middle:
  for r in outputs:assert next(c for c in r if c['vertex']==B+0x500)['attributes']==(1,11,21,31)
 assert rd(B+0x140)==0
 rows.append(dict(intermediate=middle,outputs=outputs,source_ring_empty=True))
(R/'artifacts/crater-shading-re/csg-full-split.json').write_text(json.dumps(rows,indent=2));print('PASS:2 complete original face splits with real ring mutation')
# Re-split the second output across original vertex3 and previously inserted midpoint4.
source=rd(B+0x3004);before=ring(source);assert {c['vertex'] for c in before}=={B+0x400,B+0x480,B+0x4c0,B+0x500}
for i,v in enumerate([3,4]):u.mem_write(B+0x900+i*24,w(B+0x400+v*64)+bytes(16)+f(i));u.mem_write(B+0x1100+i*4,w(B+0x900+i*24))
u.mem_write(S,w(stop,B+0x2000,0,1,B+0x1100,B+0x3010,B+0x3014));u.reg_write(UC_X86_REG_ECX,source);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4e2650,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)&255==1
repeated=[ring(rd(B+0x3010)),ring(rd(B+0x3014))];assert all(len(r)==3 for r in repeated)
for r in repeated:assert next(c for c in r if c['vertex']==B+0x500)['attributes']==(1,11,21,31)
rows.append(dict(repeated=True,input=before,outputs=repeated,source_ring_empty=rd(source+0x40)==0));assert rd(source+0x40)==0
(R/'artifacts/crater-shading-re/csg-full-split.json').write_text(json.dumps(rows,indent=2));print('PASS: repeated split preserves inserted midpoint attributes in both descendants')

