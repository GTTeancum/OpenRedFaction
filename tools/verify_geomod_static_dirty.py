"""Execute original static/dynamic dirty marker and light removal dispatch; no game build."""
from pathlib import Path
import hashlib
assert hashlib.sha256((Path(__file__).resolve().parents[1]/'Installed_Game/RF.exe').read_bytes()).hexdigest() == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836', 'Original executable differs from verified revision'
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
M=B+0x1000;FACE=B+0x2000;LIGHT=0xc4e7d8;events=[];touch=1

def hook(cpu,a,size,_):
 if a not in [B+0x9000,0x4d8660,0x4d9fc0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if a==B+0x9000: events.append('supplied_bounds_intersection');value=touch
 elif a==0x4d8660: events.append(dict(mark_light=rd(sp+4),static_argument=rd(sp+8)));value=0
 else:value=0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);rows=[]
for entry,end in [(0x4d8b8b,0x4d8bd2),(0x4d8c62,0x4d8ca9)]:
 for static in [0,1]:
  for dirty in [0,1,2,3,8]:
   for touch in [0,1]:
    events.clear();u.mem_write(S,bytes(0x1200));u.mem_write(S+0x14,w(B+0x9000));u.mem_write(S+0x11d8,w(static));u.mem_write(M+8,bytes([dirty]));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EDI,M);u.reg_write(UC_X86_REG_ESI,FACE);u.reg_write(UC_X86_REG_EBX,LIGHT);u.emu_start(entry,end,count=1000)
    expected=dirty if dirty&(2 if static else 1) or not touch else (3 if static else 1)
    actual=u.mem_read(M+8,1)[0];assert actual==expected
    rows.append(dict(entry=hex(entry),static=static,dirty_before=dirty,touches=touch,dirty_after=actual,events=events[:]))
removal=[]
for dynamic in [0,1]:
 for static in [0,1]:
  for references in [0,2]:
   events.clear();u.mem_write(LIGHT,bytes(0x10c));u.mem_write(LIGHT,w(LIGHT,LIGHT));u.mem_write(LIGHT+8,w(2));u.mem_write(LIGHT+0x4d,bytes([dynamic]));u.mem_write(LIGHT+0x58,w(references));u.mem_write(S,w(stop,0,static));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4d9130,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
   dispatch=[x for x in events if isinstance(x,dict)];assert bool(dispatch)==(references<=1 and bool(dynamic or static));assert not dispatch or dispatch[0]['static_argument']==static
   removal.append(dict(dynamic=dynamic,static=static,references=references,dispatch=dispatch))
(R/'artifacts/crater-shading-re/crater-static-dirty.json').write_text(json.dumps(dict(markers=rows,removal=removal),indent=2));print('PASS:40 marker intervals and8 whole light-removal calls')
