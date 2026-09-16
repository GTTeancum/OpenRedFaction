"""Complete original split with exact-zero versus tiny child; face disposal is recorded boundary."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_csg_full_split.py').read_text().split('rows=[]')[0])
removed=[]
def disposal(cpu,a,size,_):
 if a!=0x4cfb20:return
 sp=cpu.reg_read(UC_X86_REG_ESP);removed.append(rd(sp+4));cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+12)
u.hook_add(UC_HOOK_CODE,disposal);rows=[]
for height in [0,1e-12,1e-30]:
 u.mem_write(B,bytes(0x10000));nextface=B+0x5000;nextcorner=B+0x6000;removed.clear();u.mem_write(B+0x100,f(0,0,1,0));u.mem_write(B+0x140,w(B+0x200))
 for i,xyz in enumerate([(0,0,0),(1,-height,0),(2,0,0),(0,1,0)]):
  u.mem_write(B+0x400+i*64,f(*xyz));u.mem_write(B+0x200+i*32,w(B+0x400+i*64)+f(i,10+i,20+i,30+i)+w(B+0x200+(i+1)%4*32,B+0x200+(i-1)%4*32))
 for i,v in enumerate([0,2]):u.mem_write(B+0x800+i*24,w(B+0x400+v*64)+bytes(16)+f(i));u.mem_write(B+0x1000+i*4,w(B+0x800+i*24))
 u.mem_write(S,w(stop,B+0x2000,0,1,B+0x1000,B+0x3000,B+0x3004));u.reg_write(UC_X86_REG_ECX,B+0x100);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4e2650,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)&255==1
 outputs=[rd(B+0x3000),rd(B+0x3004)];assert bool(outputs[0])==(height!=0) and outputs[1]!=0;assert len(removed)==int(height==0);assert rd(B+0x140)==0
 rows.append(dict(height=height,child_pointers=outputs,removed=removed[:],surviving_rings=[ring(p) if p else [] for p in outputs],split_return=1,source_ring_empty=True))
(R/'artifacts/crater-shading-re/csg-split-degenerate-child.json').write_text(json.dumps(rows,indent=2));print('PASS:3 whole splits distinguish zero-area removal from tiny surviving child')
