"""Original mapping identity lifecycle:4f26a0 initial upload,4f1f30 marking,4f9d30 draw-update gate and original no-light restoration."""
from pathlib import Path
import hashlib
assert hashlib.sha256((Path(__file__).resolve().parents[1]/'Installed_Game/RF.exe').read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836', 'Original executable differs from verified revision'
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
M=B+0x1000;I=B+0x2000;RGB=B+0x3000;PIX=B+0x4000;FACE=B+0x5000;SOLID=B+0x6000;CENTER=B+0x7000;events=[]
def boundary(cpu,a,size,_):
 if a not in [0x504ab0,0x50e2e0,0x50e310,0x4d9c00,0x40a480]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);pop=0;value=0
 if a==0x50e2e0:
  events.append('lock');out=rd(sp+12);cpu.mem_write(out,bytes(28));cpu.mem_write(out+12,w(PIX));cpu.mem_write(out+24,w(4));value=1
 elif a==0x50e310:events.append('unlock')
 elif a==0x4d9c00:events.append('query_no_dynamic_sources')
 elif a==0x40a480:cpu.mem_write(B+0x8000,w(M));value=B+0x8000;pop=4
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
u.hook_add(UC_HOOK_CODE,boundary);u.mem_write(M,bytes(124));u.mem_write(M+8,b'\x08');u.mem_write(M+12,w(I,0,0,2,1));u.mem_write(M+0x68,w(0xffffffff));u.mem_write(I,w(0,2,1,RGB,7,0));base=bytes([32,64,95,80,40,56]);u.mem_write(RGB,base);u.mem_write(PIX,bytes(4));u.mem_write(FACE+0x10,f(-1,-1,-1,1,1,1));u.mem_write(FACE+0x36,b'\0\0');u.mem_write(0xc96890,b'\0');u.mem_write(0xc9b4b4,w(0));rows=[]
def call(entry,args,ecx=0):
 events.clear();u.mem_write(S,w(stop,*args));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ECX,ecx);u.emu_start(entry,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop;assert bytes(u.mem_read(RGB,6))==base
 rows.append(dict(entry=hex(entry),dirty=u.mem_read(M+8,1)[0],pixels=bytes(u.mem_read(PIX,4)).hex(),events=events[:],mapping=M,image=rd(M+12),base_rgb_unchanged=True))
call(0x4f26a0,[SOLID,0],M);initial=bytes(u.mem_read(PIX,4));assert rows[-1]['dirty']==0
u.mem_write(0x173c35c,b'\x01');u.mem_write(0x5a3ed4,w(1));call(0x4f9d30,[SOLID,M]);assert rows[-1]['events']==[]
call(0x4f1f30,[CENTER,struct.unpack('<I',f(2))[0],SOLID,FACE]);assert rows[-1]['dirty']==1
u.mem_write(0x5a3ed4,w(0));call(0x4f9d30,[SOLID,M]);assert rows[-1]['dirty']==1 and not rows[-1]['events']
u.mem_write(0x5a3ed4,w(1));u.mem_write(PIX,b'\xff'*4);call(0x4f9d30,[SOLID,M]);assert rows[-1]['dirty']==0 and bytes(u.mem_read(PIX,4))==initial;assert rows[-1]['events']==['query_no_dynamic_sources','lock','unlock']
(R/'artifacts/crater-shading-re/crater-mapping-lifecycle.json').write_text(json.dumps(rows,indent=2));print('PASS:5 original identity-preserving upload/mark/gated-update stages')
