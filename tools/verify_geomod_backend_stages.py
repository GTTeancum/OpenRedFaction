"""Original backend dispatch and complete lightmapped mode device-state calls."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace("root = Path(__file__).resolve().parents[1]","root = R"))
from unicorn.x86_const import UC_X86_REG_ECX
rows=[]
for source in [4,5]:
 for caps in [0,1]:
  mode=source|1<<5|4<<20
  word(0x1e64da0,0xffffffff);u.mem_write(0x1cfcc1d,bytes([caps]));render_calls.clear();texture_calls.clear();u.mem_write(stack,struct.pack('<II',stop,mode));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x54f160,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  rows.append(dict(source=source,modulate2x_cap=caps,mode=hex(mode),texture_calls=texture_calls[:],render_calls=render_calls[:]))
dispatched=[]
def dispatch(m,a,size,_):
 if a!=0x558960:return
 sp=m.reg_read(UC_X86_REG_ESP);dispatched.append(list(struct.unpack('<4I',m.mem_read(sp+4,16))));m.reg_write(UC_X86_REG_EIP,struct.unpack('<I',m.mem_read(sp,4))[0]);m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,dispatch);routes=[]
for backend in [0,0x65,0x66,0x67,0x68,0x6a]:
 dispatched.clear();word(0x17c7bcc,backend);u.mem_write(stack,struct.pack('<5I',stop,11,22,33,44));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x517180,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop;assert dispatched==([[11,22,33,44]] if backend==0x66 else []);routes.append(dict(backend=backend,dispatch=dispatched[:]))
initialization=[]
for requested in [0,0x65,0x66,0x67,0x68,0x6a]:
 u.mem_write(stack+0x40,struct.pack('<I',requested));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x50c25f,0x50c282,count=100);selected=struct.unpack('<I',u.mem_read(0x17c7bcc,4))[0];assert selected==(0x66 if requested==0x6a else requested);initialization.append(dict(requested=requested,selected=selected))
# Separate image sampler, same executable; only lock/release supplied.
from unicorn.x86_const import UC_X86_REG_FPCW
pixels=base+0x3000;rgba=base+0x4000;sample_events=[]
def lock_hook(m,a,size,_):
 if a not in [0x55ce00,0x50e310]:return
 sp=m.reg_read(UC_X86_REG_ESP);r=lambda off:struct.unpack('<I',m.mem_read(sp+off,4))[0]
 if a==0x55ce00:
  m.mem_write(r(12),struct.pack('<8I',0,0,5,pixels,1,1,2,0));m.reg_write(UC_X86_REG_EAX,1);sample_events.append('lock')
 else:sample_events.append('release')
 m.reg_write(UC_X86_REG_EIP,r(0));m.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,lock_hook);samples=[]
for packed in [0,0x8000,0xfc00,0x83e0,0x801f,0xffff,0x908b,0x9c85]:
 sample_events.clear();u.mem_write(pixels,struct.pack('<H',packed));u.mem_write(stack,struct.pack('<5I',stop,17,0,0,rgba));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x55cfa0,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop;assert sample_events==['lock','release'];actual=list(u.mem_read(rgba,4));assert actual==[((packed>>10)&31)*8,((packed>>5)&31)*8,(packed&31)*8,255 if packed&0x8000 else 0];samples.append(dict(packed=hex(packed),rgba=actual))
(root/'artifacts/crater-shading-re/crater-backend-stages.json').write_text(json.dumps(dict(texture_stages=rows,dispatch=routes,initialization=initialization,channels=samples),indent=2));print('PASS:4 full54f160 modes,6 full517180 dispatch cases,6 initialization intervals,8 full55cfa0 channel cases')
