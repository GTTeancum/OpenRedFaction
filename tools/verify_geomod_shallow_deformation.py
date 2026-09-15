"""Unhooked original first shallow limiting-vector deformation, supplied established stack locals."""
import sys,struct,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import *
from unicorn.x86_const import *
p=pefile.PE(str(R/'Installed_Game/RF.exe')).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(p)+4095)&~4095);u.mem_write(0x400000,p);B=0x30000000;u.mem_map(B,65536);S=B+0xe000;f=lambda *v:struct.pack('<'+'f'*len(v),*v);rows=[]
for axis in range(3):
 for sign in [-1,1]:
  for distance in [-5,-2,0,2,5]:
   center=[10,20,30];delta=[3,3,3];delta[axis]=distance;point=[a+b for a,b in zip(center,delta)];normal=[0,0,0];normal[axis]=sign
   u.mem_write(B,f(*center));u.mem_write(S,bytes(0x100));u.mem_write(S+0x14,f(5));u.mem_write(S+0x18,f(*point));u.mem_write(S+0x30,f(2));u.mem_write(S+0x44,f(*normal));u.mem_write(S+0x50,f(*delta));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBX,B);u.emu_start(0x4dc103,0x4dc190,count=10000);out=struct.unpack('<3f',u.mem_read(S+0x18,12));expected=point.copy()
   if distance*sign>0:expected[axis]=center[axis]+distance*2/5
   assert f(*out)==f(*expected),(point,normal,out,expected);rows.append(dict(center=center,input=point,unit_limit=normal,radius=5,depth=2,output=out))
(R/'artifacts/crater-shading-re/shallow-deformation.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest(),scope=__doc__,rows=rows),indent=2));print('PASS:30 unhooked original shallow deformation cases')
