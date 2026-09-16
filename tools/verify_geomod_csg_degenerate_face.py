"""Complete original4dfe20 degenerate-face validation with supplied or computed plane; no hooks."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
rows=[]
shapes=[('triangle_'+str(h),[(0,0,0),(1,0,0),(1,h,0)]) for h in [1,.0001,1e-6,1e-12,1e-20,1e-30,1e-40,0]]
shapes += [('reversed',[(0,0,0),(0,1,0),(1,0,0)]),('two',[(0,0,0),(1,0,0)]),('collinear',[(0,0,0),(1,0,0),(2,0,0)]),('duplicate_corner',[(0,0,0),(1,0,0),(1,0,0),(0,1,0)]),('bowtie',[(0,0,0),(1,1,0),(0,1,0),(1,0,0)])]
for mode in ['supplied','computed']:
 for name,points in shapes:
  u.mem_write(B,bytes(0x5000));u.mem_write(B+0x100,f(0,0,1,0));u.mem_write(B+0x140,w(B+0x200));u.mem_write(B+0x2000,f(0,0,1,0));n=len(points)
  for i,p in enumerate(points):u.mem_write(B+0x400+i*64,f(*p));u.mem_write(B+0x200+i*32,w(B+0x400+i*64)+f(i,10+i,20+i,30+i)+w(B+0x200+(i+1)%n*32,B+0x200+(i-1)%n*32))
  before=bytes(u.mem_read(B+0x200,n*32));u.mem_write(S,w(stop,B+0x2000 if mode=='supplied' else 0,0));u.reg_write(UC_X86_REG_ECX,B+0x100);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4dfe20,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;ok=u.reg_read(UC_X86_REG_EAX)&255;assert bytes(u.mem_read(B+0x200,n*32))==before
  rows.append(dict(name=name,mode=mode,accepted=ok,plane=struct.unpack('<4f',u.mem_read(B+0x100,16)),corners_unchanged=True))
assert next(r for r in rows if r['mode']=='supplied' and r['name']=='triangle_1e-40')['accepted']==1
assert all(r['accepted']==0 for r in rows if r['name'] in ['two','collinear','triangle_0','bowtie'])
(R/'artifacts/crater-shading-re/csg-degenerate-face.json').write_text(json.dumps(rows,indent=2));print('PASS:',len(rows),'whole original face validation cases')
for normal in [(1,0,0),(0,0,0),(0,0,-1),(0,0,2)]:
 u.mem_write(B+0x140,w(B+0x200));u.mem_write(B+0x2000,f(*normal,0))
 for i,p in enumerate([(0,0,0),(1,0,0),(0,1,0)]):u.mem_write(B+0x400+i*64,f(*p));u.mem_write(B+0x200+i*32,w(B+0x400+i*64)+bytes(16)+w(B+0x200+(i+1)%3*32,B+0x200+(i-1)%3*32))
 u.mem_write(S,w(stop,B+0x2000,0));u.reg_write(UC_X86_REG_ECX,B+0x100);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4dfe20,stop,count=10000);ok=u.reg_read(UC_X86_REG_EAX)&255;assert ok==int(normal[2]!=0);rows.append(dict(name='supplied_normal',normal=normal,accepted=ok))
(R/'artifacts/crater-shading-re/csg-degenerate-face.json').write_text(json.dumps(rows,indent=2));print('PASS:4 supplied-plane direction/magnitude cases')
