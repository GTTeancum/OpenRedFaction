"""Original crater position encode/decode plus duplicate gate with actual decoding."""
import sys,struct,json,hashlib,math
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import *
from unicorn.x86_const import *
p=pefile.PE(str(R/'Installed_Game/RF.exe')).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(p)+4095)&~4095);u.mem_write(0x400000,p);B=0x30000000;u.mem_map(B,65536);S=B+0xe000;STOP=B+0xf000;w=lambda *v:struct.pack('<'+'I'*len(v),*v);f=lambda *v:struct.pack('<'+'f'*len(v),*v);f32=lambda x:struct.unpack('<f',f(x))[0]
def call(a,args):
 u.mem_write(S,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(a,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
from fractions import Fraction as F
def q53(v):
 if not v:return v
 sign=-1 if v<0 else 1;v=abs(v);e=v.numerator.bit_length()-v.denominator.bit_length()
 if v<F(2)**e:e-=1
 step=F(2)**(e-52);a=v/step;n,r=divmod(a.numerator,a.denominator)
 if 2*r>a.denominator or (2*r==a.denominator and n%2):n+=1
 return sign*n*step
rows=[]
for lo,hi in [(0,65536),(-28,28),(-1000,1000)]:
 for fraction in [0,1/131072,1/65536,1.5/65536,.123,.5,.999,1]:
  xyz=list(map(f32,[lo+(hi-lo)*fraction,lo+(hi-lo)*.25,lo+(hi-lo)*.75]));u.mem_write(B+0x48,f(lo,lo,lo,hi,hi,hi));u.mem_write(B+0x200,f(*xyz));call(0x4b5820,[B,B+0x200,B+0x300]);packed=struct.unpack('<3H',u.mem_read(B+0x300,6));call(0x4b5900,[B,B+0x300,B+0x400]);decoded=struct.unpack('<3f',u.mem_read(B+0x400,12));expected=tuple(int(q53(q53(F(65536)/F(hi-lo))*F(v-lo)))&65535 for v in xyz);assert packed==expected,(xyz,packed,expected);assert decoded==tuple(f32(lo+(hi-lo)*(v/65536)) for v in packed);rows.append(dict(bounds=[lo,hi],input=xyz,packed=packed,decoded=decoded))
for xyz in [(-.001,.5,.5),(1.001,.5,.5),(.5,-.001,.5),(.5,.5,1.001)]:
 u.mem_write(B+0x48,f(0,0,0,1,1,1));u.mem_write(B+0x200,f(*xyz));call(0x4b5820,[B,B+0x200,B+0x300]);assert bytes(u.mem_read(B+0x300,6))==bytes(6);rows.append(dict(bounds=[0,1],input=xyz,packed=[0,0,0]))
reached=[]
def hook(cpu,a,size,_):
 if a in [0x467155,0x46726a]:reached.append(a);cpu.emu_stop()
u.hook_add(UC_HOOK_CODE,hook);duplicates=[]
for lo,hi,old,new in [(-1000,1000,.015,.205),(-1000,1000,.03,-.18),(-28,28,.0001,.2001),(-28,28,.1,.3),(0,65536,65536,65535.9)]:
 u.mem_write(B+0x48,f(lo,lo,lo,hi,hi,hi));u.mem_write(B+0x200,f(old,0,0));call(0x4b5820,[B,B+0x200,0x648608]);call(0x4b5900,[B,0x648608,B+0x400]);decoded=struct.unpack('<3f',u.mem_read(B+0x400,12));u.mem_write(0x6460e8,w(B));u.mem_write(0x647c9c,w(1));u.mem_write(0x648600,struct.pack('<HHI',7,0,11));u.mem_write(S+0x24,w(7,11));u.mem_write(B+0x500,f(new,0,0));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBP,B+0x500);reached.clear();u.emu_start(0x4671f7,0x46726b,count=10000);duplicates.append(dict(bounds=[lo,hi],old_input=f32(old),decoded=decoded,new_input=f32(new),duplicate_rejected=reached==[0x467155]))
# Main mode matches the original startup53-bit precision.
precision=[]
for cw in [0x27f,0x37f]:
 u.mem_write(B+0x48,f(-1000,-1000,-1000,1000,1000,1000));u.mem_write(B+0x200,f(-500,0,500));u.mem_write(S,w(STOP,B,B+0x200,B+0x300));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,cw);u.emu_start(0x4b5820,STOP,count=10000);precision.append(dict(control_word=hex(cw),input=[-500,0,500],packed=struct.unpack('<3H',u.mem_read(B+0x300,6))))
assert precision[0]['packed']!=(precision[1]['packed'])
(R/'artifacts/crater-shading-re/crater-position-53bit.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest(),scope=__doc__,encode_decode=rows,duplicates=duplicates,precision=precision,main_control_word="0x27f"),indent=2));print('PASS:28 original encoding cases +5 actual-decoding duplicate gates')

import subprocess
for row in rows:
 lo,hi=row['bounds'];data=[lo]*3+[hi]*3+list(row['input'])
 got=subprocess.check_output([str(R/'build/pc/Release/rf_geomod_basis_probe.exe'),'--crater-position'],input=' '.join(map(str,data)),text=True).split()
 assert tuple(map(int,got[:3]))==tuple(row['packed']),(row,got)
 if 'decoded' in row:assert f(*map(float,got[3:]))==f(*row['decoded']),(row,got)
print('PASS:28 shared C/original53-bit position codec cases')
