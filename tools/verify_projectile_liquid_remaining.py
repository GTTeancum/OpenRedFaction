"""Original liquid-contact dispatcher numerical evidence; no native game launch."""
import sys,struct,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
exe=R/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image()
B=0x30000000;S=B+0xe0000;stop=B+0xf0000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rd=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
(R/'artifacts/crater-shading-re').mkdir(parents=True,exist_ok=True)
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000);rows=[]
def hook(cpu,a,n,_):
 if a in [0x467020,0x4c4ec0,0x4c59f0]:raise AssertionError(hex(a))
 if a in [0x49cd30,0x4c16e0,0x40a490]:
  sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EAX,B+0x2000 if a==0x40a490 else 0);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook)
for length in [1,10]:
 for fraction in [0,.001,.25,.9]:
  u.mem_write(B,bytes(0x3000));u.mem_write(B+0x24,w(2));u.mem_write(B+0x294,w(B+0x1000));u.mem_write(B+0x34,f(10));u.mem_write(B+0x78,f(.1));u.mem_write(B+0xe4,f(0,0,0));u.mem_write(B+0xf0,f(length,0,0));u.mem_write(B+0xfc,f(1,0,0,0,1,0,0,0,1));u.mem_write(B+0x1b0,f(.1));u.mem_write(B+0x1cc,f(fraction));u.mem_write(B+0x1c0,f(0,1,0));u.mem_write(B+0x1ec,w(1));u.mem_write(B+0x1ac,w(0x1000));u.mem_write(B+0x298,w(7));u.mem_write(0x872118,w(88));u.mem_write(S,w(stop,B));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4a01b0,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop
  pos=struct.unpack('<f',u.mem_read(B+0xe4,4))[0];remaining=struct.unpack('<f',u.mem_read(B+0x1b0,4))[0];assert abs(pos-max(0,length*fraction-.05))<1e-6;assert abs(remaining-.1*(1-fraction))<1e-7
  assert rd(B+0x1ac)==0 and rd(B+0x1ec)==0
  rows.append(dict(path_length=length,fraction=fraction,advanced=pos,remaining=remaining,query_after=rd(B+0x1ac)))
(R/'artifacts/crater-shading-re/geomod-liquid-remaining-time.json').write_text(json.dumps(rows,indent=2));print('PASS',len(rows),'whole original movement plus liquid callback cases')
# Original next-iteration position prediction with zero acceleration.
predictions=[]
for remaining in [.1,.075,.01]:
 u.mem_write(B+0xe4,f(2.45,0,0));u.mem_write(B+0x144,f(100,0,0));u.mem_write(B+0x1b0,f(remaining));u.mem_write(0x7c7048,f(0,0,0));u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x49fb77,0x49fbe6,count=1000)
 endpoint=struct.unpack('<f',u.mem_read(B+0xf0,4))[0];assert abs(endpoint-(2.45+100*remaining))<2e-6;predictions.append(dict(remaining=remaining,endpoint=endpoint))
(R/'artifacts/crater-shading-re/geomod-liquid-next-prediction.json').write_text(json.dumps(predictions,indent=2));print('PASS',len(predictions),'original next-iteration prediction cases')
# Exactly-one hit fraction is the no-contact tail: callback not invoked.
u.mem_write(B,bytes(0x3000));u.mem_write(B+0x24,w(2));u.mem_write(B+0x294,w(B+0x1000));u.mem_write(B+0xe4,f(0,0,0));u.mem_write(B+0xf0,f(10,0,0));u.mem_write(B+0x120,f(1,0,0,0,1,0,0,0,1));u.mem_write(B+0x1b0,f(.1));u.mem_write(B+0x1cc,f(1));u.mem_write(B+0x1ec,w(1));u.mem_write(B+0x1ac,w(0x1000));u.mem_write(S,w(stop,B));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x4a01b0,stop,count=100000)
assert u.reg_read(UC_X86_REG_EIP)==stop
assert struct.unpack('<f',u.mem_read(B+0xe4,4))[0]==10 and rd(B+0x1b0)==0 and rd(B+0x1ec)==1 and rd(B+0x1ac)==0x1000
print('PASS fraction1 commits endpoint and skips liquid callback; liquid/query unchanged')

# Compare the reconstructed shared advance against the original liquid route.
import subprocess
payload=b''.join(struct.pack('<8fI',.1,r['fraction'],0,0,0,r['path_length'],0,0,0) for r in rows)
output=subprocess.check_output([str(R/'build/pc/Release/rf_physics_probe.exe'),'--weapon-advance'],input=payload)
assert len(output)==len(rows)*20
for index,row in enumerate(rows):
 x,y,z,adjusted,left=struct.unpack_from('<5f',output,index*20)
 assert struct.pack('<f',x)==struct.pack('<f',row['advanced']),(index,x,row)
 assert struct.pack('<f',left)==struct.pack('<f',row['remaining']),(index,left,row)
 assert y==z==0
print('PASS',len(rows),'bit-exact shared C position/remaining-time comparisons')

# Execute the freshly linked NXDK helper too; no emulator/display needed.
import re
xp=pefile.PE(str(R/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(B,0x100000)
entry=int(re.search(r'_rf_physics_weapon_contact_advance\s+([0-9a-fA-F]+)',(R/'build/xbox/main.map').read_text())[1],16)
for row in rows:
 state=bytearray(308);struct.pack_into('<3f',state,100,row['path_length'],0,0)
 x.mem_write(B,bytes(state));x.mem_write(B+0x2000,bytes(4));x.mem_write(S,w(stop,B)+f(.1,row['fraction'])+w(B+0x2000))
 x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,(row,hex(x.reg_read(UC_X86_REG_EIP)),x.reg_read(UC_X86_REG_EAX))
 assert bytes(x.mem_read(B+88,12))==f(row['advanced'],0,0)
 assert bytes(x.mem_read(B+0x2000,4))==f(row['remaining'])
print('PASS',len(rows),'bit-exact linked NXDK position/remaining-time comparisons')
