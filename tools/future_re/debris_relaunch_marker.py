"""Original48fe30 existing-fragment relaunch prepass; no new debris creation."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
exec((R/'tools/verify_particle_duration.py').read_text().split('u.hook_add')[0].replace('root=Path(__file__).resolve().parents[1]','root=R'))
actor=base+0x2000;origin=base+0x4000;sentinel=0x75eed8
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
get=lambda p:struct.unpack('<I',u.mem_read(p,4))[0]
def hook(cpu,a,size,_):
 if a==0x577eef:thread_data(cpu,a,size,_)
 elif a==0x48fe9b:cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for marker,distance,count,seed in itertools.product([0,base+0x5000,0xdeadbeef],[(1.,0,0),(2.,0,0),(3.,0,0),(1.2,1.2,1.2),(-1.2,-1.2,-1.2),(1.5,1.5,0)],[0,1,4],[0,1,0xffffffff]):
 u.mem_write(actor,bytes(0x200));u.mem_write(actor,w(sentinel,sentinel));u.mem_write(sentinel,w(actor,actor));u.mem_write(actor+8,f(*distance));u.mem_write(actor+0x38,f(.1,.25));u.mem_write(actor+0x48,f(7,8,9));u.mem_write(actor+0x64,w(count,marker));u.mem_write(actor+0x70,f(2.75,2.5));u.mem_write(origin,f(0,0,0));u.mem_write(thread+20,w(seed));u.mem_write(stack,w(stop,origin)+f(2)+w(0,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);draws=0
 before=bytes(u.mem_read(actor,0x7c));u.emu_start(0x48fe30,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x48fe9b
 after=bytes(u.mem_read(actor,0x7c));a,b,c=sorted([abs(struct.unpack('<f',f(v))[0]) for v in distance],reverse=True);approx=a+1.5*(b*.25+c*.125);eligible=marker==0 and approx<2
 assert draws==(3 if eligible else 0)
 if eligible:
  assert get(actor+0x64) in (3,4);assert after[0x48:0x54]!=before[0x48:0x54]
  assert all(after[i]==before[i] for i in range(0x7c) if not 0x48<=i<0x54 and not 0x64<=i<0x68)
 else:assert before==after
 rows.append(dict(marker=marker,position=distance,approx_distance=approx,bounces_before=count,bounces_after=get(actor+0x64),seed=seed,next_seed=get(thread+20),draws=draws,eligible=eligible,velocity=struct.unpack('<3f',after[0x48:0x54]),age=struct.unpack('<f',after[0x70:0x74])[0]))
(R/'artifacts/future-vehicles-re/debris-relaunch-marker.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original relaunch marker/radius/count cases')
