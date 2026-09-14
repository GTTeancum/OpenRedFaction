"""Compiled NXDK v180 ambient field binding, with archive reads supplied."""
import json,random,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(im)+4095)//4096*4096);x.mem_write(base,im)
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000;x.mem_map(B,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16)
data=b''
def archive_read(u,a,size,context):
 sp=u.reg_read(UC_X86_REG_ESP);ret,_,_,offset,dest,count=struct.unpack('<6I',u.mem_read(sp,24));offset-=8
 assert offset<=len(data) and count<=len(data)-offset
 if count:u.mem_write(dest,data[offset:offset+count])
 u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,archive_read,begin=sym('rf_vpp_read'),end=sym('rf_vpp_read'))
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x4618b0);guards=0
for i in range(1024):
 name=rng.randbytes(i%71);color=rng.randbytes(4);mode=i%256
 data=struct.pack('<H',len(name))+name+w(55)+color+bytes([mode])+rng.randbytes(12)
 x.mem_write(B,bytes(4096));x.mem_write(B,w(B+0x1000));x.mem_write(B+72,w(len(data)+8,180,0,1));x.mem_write(B+600,w(0x900,0,len(data)));x.mem_write(B+0x2000,b'\xa5'*8)
 assert call('rf_level_lighting_read',[B,B+0x2000])==0 and bytes(x.mem_read(B+0x2000,5))==color+bytes([mode])
 x.mem_write(B+608,w(1));x.mem_write(B+0x2000,b'\xa5'*8)
 assert call('rf_level_lighting_read',[B,B+0x2000])!=0 and bytes(x.mem_read(B+0x2000,8))==b'\xa5'*8;guards+=1
 raw=bytearray(40);raw[32]=i%2;raw[33]=mode
 raw+=struct.pack('<H',len(name))+name
 if raw[32]:raw+=rng.randbytes(8)+struct.pack('<H',len(name))+name+rng.randbytes(37)
 if mode:raw+=color
 x.mem_write(B+0x3000,bytes(raw));x.mem_write(B+0x1000,w(B+0x3000,len(raw),0,0,1,0,0,0,0,0,0,0,0,B+0x1800,0,0,0));x.mem_write(B+0x1800,w(0));x.mem_write(B+0x2000,b'\xa5'*8)
 assert call('rf_geometry_room_ambient',[B+0x1000,0,B+0x2000])==0 and bytes(x.mem_read(B+0x2000,4))==bytes([mode])+(color[:3] if mode else bytes(3))
 x.mem_write(B+0x1004,w(39));x.mem_write(B+0x2000,b'\xa5'*8)
 assert call('rf_geometry_room_ambient',[B+0x1000,0,B+0x2000])!=0 and bytes(x.mem_read(B+0x2000,8))==b'\xa5'*8;guards+=1
 assert call('rf_geometry_room_ambient',[B+0x1000,0xffffffff,B+0x2000])==0 and bytes(x.mem_read(B+0x2000,4))==bytes(4)
report=dict(result='PASS',nxdk_level_records=1024,nxdk_room_records=1024,truncation_guards=guards,scope='Independent serialized field expectations, variable strings, liquid room payloads, all override/directional byte values and absent rooms. Archive I/O supplied; not original full-loader execution or native Xbox evidence. Field provenance4618b0/4ed520; campaign fixture provides separate PC real-data integration.')
(root/'artifacts/ambient-binding.json').write_text(json.dumps(report,indent=2));print(report)
