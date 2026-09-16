"""Original prepared single-fragment48ffa0..4900e4 with real birth/math/RNG."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
exec((R/'tools/verify_particle_duration.py').read_text().split('u.hook_add')[0].replace('root=Path(__file__).resolve().parents[1]','root=R'))
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EBX
actor=base+0x2000;room=base+0x6000
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
get=lambda p:struct.unpack('<I',u.mem_read(p,4))[0]
checkpoints={}
def hook(cpu,a,size,_):
 if a==0x577eef:thread_data(cpu,a,size,_);return
 if a==0x4900e4:cpu.emu_stop();return
 if a in (0x490043,0x490081,0x4900bc,0x4900cb,0x490230,0x490150):checkpoints[hex(a)]=dict(draws=draws,state=get(thread+20))
 if a not in (0x490100,0x48fc10,0x510630):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if a==0x510630:cpu.mem_write(get(sp+8),w(256));cpu.mem_write(get(sp+12),w(256));result=0
 else:result=actor if a==0x490100 else 0
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for seed,radius in itertools.product([0,1,0xffffffff,1234567,7654321],[.5,1,2,4]):
 u.mem_write(actor,bytes(0x1000));u.mem_write(stack,bytes(0x100));u.mem_write(stack+0x10,f(0,0,0));u.mem_write(stack+0x64,w(1));u.mem_write(stack+0x68,f(radius));u.mem_write(thread+20,w(seed));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,101);u.reg_write(UC_X86_REG_EBX,room);u.reg_write(UC_X86_REG_FPCW,0x27f);draws=0;checkpoints={}
 u.emu_start(0x48ffa0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x4900e4;assert draws==35,(seed,radius,draws)
 readf=lambda at,n:list(struct.unpack('<'+'f'*n,u.mem_read(actor+at,n*4)))
 rows.append(dict(seed=seed,blast_radius=radius,next_seed=get(thread+20),checkpoints=checkpoints,position=readf(8,3),radius=readf(0x38,1)[0],resistance=readf(0x3c,1)[0],bounces=get(actor+0x64),axis=readf(0x54,3),spin=readf(0x60,1)[0],flags=get(actor+0x78),age=readf(0x70,1)[0],lifetime=readf(0x74,1)[0],velocity=readf(0x48,3)))
(R/'artifacts/future-vehicles-re/debris-birth.json').write_text(json.dumps(dict(exe_sha256=sha,scope=__doc__,cases=rows),indent=2)+'\n');print('PASS',len(rows),'original full35-draw births')
