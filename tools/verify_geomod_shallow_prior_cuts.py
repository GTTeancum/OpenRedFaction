"""Whole original45cff0 prior-cut alignment: real math; only region container and template getter supplied."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
def shape_hook(cpu,a,size,_):
 if a!=0x4375b0:return
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,B+0x9000);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,shape_hook)
def norm(v):
 d=math.sqrt(sum(x*x for x in v));return tuple(x/d for x in v) if d else (0,0,0)
def expected(ns,old):
 corr=[None]*len(ns)
 for pos,limits,scale in old:
  if not any(limits[0]) or sum(x*x for x in pos)>=(5*struct.unpack('<f',f(scale))[0])**2:continue
  direction=norm(tuple(-x for x in pos))
  for limit in limits:
   depth=math.sqrt(sum(x*x for x in limit));n=norm(limit)
   for j,newnormal in enumerate(ns):
    if corr[j] is not None:continue
    if sum(a*b for a,b in zip(n,tuple(-x for x in newnormal)))<.95:continue
    if sum(a*b for a,b in zip(n,direction))<=0:continue
    signed=sum(-a*b for a,b in zip(pos,n))
    if signed*signed<depth*depth:corr[j]=tuple(-signed*x for x in n)
 return tuple(sum(c[k] for c in corr if c is not None) for k in range(3))
def run(name,ns,old):
 global regions
 regions=ns;u.mem_write(B,bytes(0x10000));u.mem_write(B+0x103c,f(5));u.mem_write(B+0x9060,f(5));u.mem_write(0x647c9c,w(len(old)))
 for i,n in enumerate(ns):
  a=B+0x3000+i*0x50;u.mem_write(B+0x2000+i*4,w(a));u.mem_write(a,w(2,25)+bytes([1,0,0,0])+f(2+i,0,0,0,1,0,0,*n,0,0,1,100,100,100,100))
 for i,(pos,limits,scale) in enumerate(old):
  u.mem_write(0x646a28+i*36,f(*pos,*limits[0],*limits[1]));u.mem_write(0x64861c+i*32,f(scale))
 u.mem_write(S,w(stop,B,B+0x1000,1));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x45cff0,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)&255==1
 center=struct.unpack('<3f',u.mem_read(B+0x1008,12));want=expected(ns,old)
 assert all(abs(a-b)<2e-5 for a,b in zip(center,want)),(name,center,want)
 rows.append(dict(name=name,normals=ns,history=old,center=center,expected=want,limits=struct.unpack('<6f',u.mem_read(B+0x104c,24))))
z=(0,0,0);down=(0,-2,0);up=(0,1,0)
for y in [-4,-2,-1,0,.125,.5,1,1.999,2,2.001,4]:
 for lim in [down,(2,0,0),(0,2,0),z]:run('depth_side', [up],[((0,y,0),(lim,z),1)])
for x in [0,3,4.8,4.9,5,6]:run('sphere_overlap',[up],[((x,1,0),(down,z),1)])
for scale in [.1,.199,.2,.201,1]:run('old_scale',[up],[((0,1,0),(down,z),scale)])
for ys in [[.5,1],[1,.5],[-1,1],[2,1],[1,2]]:run('history_order',[up],[((0,y,0),(down,z),1) for y in ys])
for dot in [.94,.9499,.9501,.96,1]:
 n=(math.sqrt(1-dot*dot),-dot,0);run('normal_match',[up],[((0,1,0),(tuple(2*x for x in n),z),1)])
run('both_limits_same_record',[up,(1,0,0)],[((1,1,0),(down,(-2,0,0)),1)])
run('both_limits_separate_records',[up,(1,0,0)],[((0,1,0),(down,z),1),((1,0,0),((-2,0,0),z),1)])
run('zero_first_ignores_second',[up],[((0,1,0),(z,down),1)])
run('second_old_limit',[up],[((0,1,0),((2,0,0),down),1)])
(R/'artifacts/crater-shading-re/shallow-prior-cuts.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest(),scope=__doc__,rows=rows),indent=2));print('PASS:',len(rows),'whole original45cff0 prior-cut cases')


import subprocess
maximum=0
for row in rows:
 values=[len(row['normals']),len(row['history']),5,0,0,0]
 for i,n in enumerate(row['normals']):values.extend([-x for x in n]);values.append(2+i)
 for pos,limits,scale in row['history']:values.extend([*pos,*limits[0],*limits[1],scale])
 actual=list(map(float,subprocess.check_output([str(R/'build/pc/Release/rf_geomod_basis_probe.exe'),'--shallow-align'],input=' '.join(map(str,values)),text=True).split()))
 error=max(abs(a-b) for a,b in zip(actual,row['center']));maximum=max(maximum,error)
 assert error<2e-5,(row,actual,error)
print('PASS:69 shared C/original alignment cases; max error',maximum)
