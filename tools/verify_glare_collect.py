"""Original glare collector block with actual queue/frustum versus PC/NXDK."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x40000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_collect\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
B=0x30000000;O=B;D=B+0x1000;F=B+0x2000;COUNT=B+0x3000;ACCEPT=COUNT+4;RECORDS=B+0x10000;S=B+0x30000;STOP=S+0x1000
initial=b'\xa5'*(2048*48);accepted=0;room=0;room_calls=0

def hook(cpu,address,length,context):
 global accepted,room_calls
 if address in (0x4884c3,0x4884df):accepted=cpu.reg_read(UC_X86_REG_EAX)&255;return
 room_calls+=1;sp=cpu.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',cpu.mem_read(sp,4))[0]
 cpu.reg_write(UC_X86_REG_EAX,room);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
for address in (0x40a490,0x4884c3,0x4884df):u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
u.mem_write(0x88fd1c,b'\0');u.mem_write(0x9bb56c,w(0));u.mem_write(0x87bb00,bytes(12));commands=bytearray();results=bytearray();cases=0;appends=accepted_only=skipped=0
for case in range(336):
 count=(0,1,2047,2048)[case%4];planes=(case//4)%7;volume=(-1,0,1,17)[(case//7)%4]
 active=(0,1,2,255)[(case//3)%4];room=(0,123)[(case//5)%2];current=(0,123)[(case//11)%2];flags=(0,2,0x80000002)[case%3]
 planes=0 if case>=280 else planes
 position=(1.,2.,-3.);radius=(-1.,0.,.25,2.)[case%4];frustum=bytearray(156)
 for i in range(6):
  normal=[0.,0.,0.];normal[i//2]=(-1.,1.)[i%2];distance=-sum(a*b for a,b in zip(normal,position))+radius+(-.125,0,.125)[(case+i)%3]
  plane=struct.pack('<4fI',*normal,distance,0);frustum[i*20:(i+1)*20]=plane;u.mem_write(0x1818a6c+i*28,plane+bytes(8))
 struct.pack_into('<I',frustum,144,planes);u.mem_write(0x1818b8c,w(planes))
 header=w(room,current,volume,0x488b00,count,active,flags,55)+struct.pack('<4f',radius,*position);commands+=header+frustum
 original=bytearray(b'\xa5'*748);original[0x2c:0x30]=w(55);original[0x3c:0x48]=struct.pack('<3f',*position);original[0x78:0x7c]=struct.pack('<f',radius);original[0x28c]=active;original[0x2ac:0x2b0]=w(D);original[0x2b4:0x2bc]=w(flags,0x5c9ba8)
 u.mem_write(O,bytes(original));u.mem_write(D+0x24,w(volume));u.mem_write(0x5c9e60,w(O));u.mem_write(0x88fd20,initial);u.mem_write(0x9bb550,w(count));u.mem_write(0x9bb568,w(0));accepted=room_calls=0
 u.mem_write(S,w(STOP));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBP,current);u.emu_start(0x488467,0x4884fa,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==0x4884fa and room_calls==1
 after=struct.unpack('<I',u.mem_read(0x9bb550,4))[0];finalflags=struct.unpack('<I',u.mem_read(O+0x2b4,4))[0]
 expected=w(0,accepted,after,finalflags)+bytes(u.mem_read(0x88fd20,len(initial)));results+=expected
 original[0x2b4:0x2b8]=w(finalflags);assert bytes(u.mem_read(O,748))==original
 owner=bytearray(528);owner[8]=active;owner[48:52]=w(flags);owner[112:116]=w(55);owner[144:148]=struct.pack('<f',radius);owner[152:164]=struct.pack('<3f',*position)
 x.mem_write(O,bytes(owner));x.mem_write(F,bytes(frustum));x.mem_write(COUNT,w(count,0xa5a5a5a5));x.mem_write(RECORDS,initial)
 x.mem_write(S,w(STOP,O,room,current,volume,0x488b00,F,O+152,RECORDS,2048,COUNT,ACCEPT));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(ACCEPT,4))+bytes(x.mem_read(COUNT,4))+bytes(x.mem_read(O+48,4))+bytes(x.mem_read(RECORDS,len(initial)));assert actual==expected,case
 owner[48:52]=w(finalflags);assert bytes(x.mem_read(O,528))==owner
 appends+=after>count;accepted_only+=accepted and after==count;skipped+=room!=current or not active;cases+=1
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-collect'],input=commands);assert pc==results
assert appends and accepted_only and skipped
report=dict(result='PASS',cases=cases,appended=appends,accepted_without_append=accepted_only,room_or_inactive_skips=skipped,original_sha256=digest,scope='Actual original488467..4884fa glare collection loop and unhooked4d3560/5186a0 queue/culling, only room resolver supplied. PC/compiled NXDK compare acceptance, marker, all2048 queue records and owner footprints. Zero resolved world offset; no instance transform, complete room collection or draw callback claim.')
(root/'artifacts/glare-collect.json').write_text(json.dumps(report,indent=2));print(report)
