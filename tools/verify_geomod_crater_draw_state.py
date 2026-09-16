"""Original ordinary-face draw-state4f1432..4f15a3 across source/generated/detail flags; mapping lookup supplied."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
def lookup(cpu,a,size,_):
 if a!=0x40a480:return
 sp=cpu.reg_read(UC_X86_REG_ESP);assert rd(sp+4)==0;cpu.reg_write(UC_X86_REG_EAX,B+0x3000);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+8)
u.hook_add(UC_HOOK_CODE,lookup);u.mem_write(S,w(stop));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x515730,stop,count=1000);rows=[]
for flags in [0,0x100,0x108,0x300]:
 for mapping in [0,-1]:
  for multitexture in [0,1]:
   u.mem_write(B,bytes(0x4000));u.mem_write(B+0x28,w(flags));u.mem_write(B+0x30,w(101));u.mem_write(B+0x36,struct.pack('<h',mapping));u.mem_write(B+0x3000,w(B+0x3100));u.mem_write(B+0x310c,w(B+0x3200));u.mem_write(B+0x3210,w(202));u.mem_write(S,bytes(0x200));u.mem_write(S+0x1f,bytes([multitexture]));u.mem_write(S+0xb0,w(B+0x2000));u.mem_write(0x9bb5a4,w(0));u.mem_write(0x9bb5a8,w(0));u.reg_write(UC_X86_REG_EDI,B);u.reg_write(UC_X86_REG_ESI,B+0x1000);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4f1432,0x4f15a3,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4f15a3
   state=[rd(B+0x1020),rd(B+0x1024),rd(B+0x1040),rd(B+0x1044)];assert state[1:]==[0xffffffff,101,202 if mapping==0 else 0xffffffff];rows.append(dict(flags=hex(flags),mapping=mapping,multitexture=multitexture,mode=hex(state[0]),rgba=hex(state[1]),base=state[2],lightmap=state[3]))
for mapping in [0,-1]:
 for multi in [0,1]:assert len({r['mode'] for r in rows if r['mapping']==mapping and r['multitexture']==multi})==1
(R/'artifacts/crater-shading-re/crater-draw-state.json').write_text(json.dumps(rows,indent=2));print('PASS:16 ordinary/source/generated draw-state cases; flags do not alter gain or texture selection')
