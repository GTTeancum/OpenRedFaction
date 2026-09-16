"""Execute cached room mode and paired texture binding interval with real54f160."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace("root = Path(__file__).resolve().parents[1]","root = R"))
HEADER=base+0x3000;GROUP=base+0x4000;bindings=[]
def binding(m,a,size,_):
 if a!=0x55cad0:return
 sp=m.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<6I',m.mem_read(sp+4,24)));bindings.append(args[:3]);m.mem_write(args[3],struct.pack('<f',1));m.mem_write(args[4],struct.pack('<f',1));m.reg_write(UC_X86_REG_EAX,1);m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,binding);rows=[]
for same_mode in [0,1]:
 for same_base in [0,1]:
  mode=0x400024;word(0x1e64da0,mode if same_mode else 0xffffffff);u.mem_write(0x1cfcc1d,b'\1');word(HEADER+0x14,GROUP);word(GROUP+0x20,mode);word(GROUP+0x40,47);word(GROUP+0x44,17);u.mem_write(stack,bytes(0x100));word(stack+0x28,HEADER);word(stack+0x50,0);word(stack+0x44,47 if same_base else 0xffffffff);bindings.clear();texture_calls.clear();render_calls.clear();u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x55ffe0,0x560059,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x560059;assert bindings==([[1,17,0]] if same_base else [[0,47,0],[1,17,0]]);assert bool(texture_calls)==(not same_mode);rows.append(dict(same_mode=same_mode,same_base=same_base,bindings=bindings[:],texture_state_calls=texture_calls[:]))
(root/'artifacts/crater-shading-re/crater-cached-submission.json').write_text(json.dumps(rows,indent=2));print('PASS:4 cached-room binding/mode intervals with full54f160')
