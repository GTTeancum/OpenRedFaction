"""Compiled NXDK glare binding/lifetime with supplied animation-loader boundaries."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x800000);O=B+0x1000;D=B+0x2000;A=B+0x100000;S=B+0x700000;STOP=S+0x1000
sym=(root/'build/xbox/main.map').read_text();symbol=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',sym)[1],16)
entry,close,calloc,free,anim_open,anim_close=map(symbol,('rf_glare_materials_open','rf_glare_materials_close','calloc','free','rf_particle_animation_open','rf_particle_animation_close'))
r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def text(a):
 out=bytearray()
 while x.mem_read(a,1)!=b'\0':out+=x.mem_read(a,1);a+=1
 return out.decode()
audit=json.loads((root/'artifacts/glare-bitmaps/report.json').read_text())
lookup={t['name'].lower():t['candidates'][0] for t in audit['textures']}
# rf_particle_definition: 19 floats, cycle16, flags12 precede bitmap.
bitmap_offset=104
live={};animations={};calls=[];fail_alloc=False;fail_at=-1

def hook(cpu,address,length,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+4*i);result=0
 if address==calloc:
  n=arg(0)*arg(1);assert not live and 0<n<65536
  if not fail_alloc:live[A]=n;cpu.mem_write(A,bytes(n));result=A
 elif address==free:
  if arg(0):
   if arg(0) in live:n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
   else:
    # Compiler may inline animation_close: its zero-pixel descriptor array
    # is retired directly. Track that supplied allocation exactly once.
    key=next(k for k,v in animations.items() if struct.unpack_from('<I',v)[0]==arg(0))
    animations.pop(key)
 elif address==anim_open:
  name=text(arg(1)+bitmap_offset);assert name.lower() in lookup
  data=lookup[name.lower()];calls.append(name);assert arg(2)==B and arg(3)==1
  if len(calls)-1==fail_at:result=0xffffffff
  elif arg(4)<data['resident_bytes']:result=0xfffffffc
  else:
   assert arg(0) not in animations
   payload=w(B+0x500000+len(calls)*0x1000,data['frames'],data['rate'],0,data['resident_bytes'])
   animations[arg(0)]=payload;cpu.mem_write(arg(0),payload);cpu.mem_write(struct.unpack_from('<I',payload)[0],bytes(data['frames']*20))
 else:
  if arg(0) in animations:
   assert bytes(cpu.mem_read(arg(0),20))==animations.pop(arg(0))
  else:assert bytes(cpu.mem_read(arg(0),20))==bytes(20)
  cpu.mem_write(arg(0),bytes(20))
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for address in (calloc,free,anim_open,anim_close):x.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def call(function,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(function,STOP,count=10000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--glare-classes',str(root/'Installed_Game/tables.vpp'),'1000000'])[16:]
cases=0

def check(data,budget=4000000,success=True):
 global calls,cases
 assert not live and not animations;calls=[];x.mem_write(O,bytes(24));x.mem_write(D,data or b'\0');count=len(data)//300
 status=call(entry,O,D,count,B,1,budget)
 if not success:
  assert status and not live and not animations and bytes(x.mem_read(O,24))==bytes(24)
 else:
  assert not status,hex(status)
  base=24+count*(12+3*84);expected_names=[];expected_bindings=[]
  for i in range(count):
   row=data[300*i:300*(i+1)];fields=struct.unpack_from('<I',row,296)[0]
   for j in range(3):
    if not fields&(1<<j):expected_bindings.append(0xffffffff);continue
    name=row[64*(j+1):64*(j+2)].split(b'\0',1)[0].decode()
    keys=[v.lower() for v in expected_names]
    if name.lower() not in keys:expected_names.append(name);keys.append(name.lower())
    expected_bindings.append(keys.index(name.lower()))
  assert calls==expected_names
  resident=base+sum(lookup[n.lower()]['resident_bytes']-20 for n in calls)
  assert bytes(x.mem_read(O+12,12))==w(count,len(calls),resident)
  if count:assert bytes(x.mem_read(r(O+4),count*12))==w(*expected_bindings)
  for i,name in enumerate(calls):assert text(r(O+8)+84*i)==name
  before=bytes(x.mem_read(O,24));assert call(entry,O,D,count,B,1,budget)!=0 and bytes(x.mem_read(O,24))==before
 call(close,O);call(close,O);assert not live and not animations and bytes(x.mem_read(O,24))==bytes(24);cases+=1
check(raw);check(raw,1785520);check(raw,1785519,False)
for fail_at in range(38):check(raw,success=False)
fail_at=-1;fail_alloc=True;check(raw,success=False);fail_alloc=False
# Duplicate resource references across all three slots and case variants.
row=bytearray(300);name=audit['textures'][0]['name'].encode()
for j in range(3):row[64*(j+1):64*(j+1)+len(name)]=name.upper() if j==1 else name
row[296:300]=w(7);check(bytes(row)*2)
row[64:128]=b'x'*64;check(bytes(row),success=False)
row=bytearray(300);row[296:300]=w(8);check(bytes(row),success=False)
check(bytes(65*300),success=False);check(b'');check(bytes(64*300))
report=dict(result='PASS',cases=cases,scope='Actual compiled NXDK glare-material binding, deduplication, budgets, every texture-load failure, array-allocation failure, malformed fields/names, class cap and ordered retirement. Animation open/close and heap are supplied boundaries using audited PC resource sizes; this does not verify Xbox image decoding, swizzling, actual residency or native XEMU rendering.')
(root/'artifacts/glare-materials-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
