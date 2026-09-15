"""Both original shallow-deformation branches in sequence; actual helpers, no hooks."""
import sys,struct,json,hashlib,math
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import *
from unicorn.x86_const import *
p=pefile.PE(str(R/'Installed_Game/RF.exe')).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(p)+4095)&~4095);u.mem_write(0x400000,p);B=0x30000000;u.mem_map(B,65536);S=B+0xe000;f=lambda *v:struct.pack('<'+'f'*len(v),*v);f32=lambda x:struct.unpack('<f',f(x))[0];rows=[];sign_crossings=0
pairs=[((0,-1,0),2,(1,0,0),3),((0,-1,0),2,(math.sqrt(.75),-.5,0),3),((math.sqrt(.75),-.5,0),3,(0,-1,0),2),((0,-1,0),2,(math.sqrt(.9975),.05,0),3)]
for n1,d1,n2,d2 in pairs:
 n1=tuple(map(f32,n1));n2=tuple(map(f32,n2))
 for x in [-5,-2,0,2,5]:
  for y in [-5,-2,0,2,5]:
   center=[10,20,30];offset=[x,y,4];point=[a+b for a,b in zip(center,offset)];u.mem_write(B,f(*center));u.mem_write(S,bytes(0x180));u.mem_write(S+0x14,f(5));u.mem_write(S+0x18,f(*point));u.mem_write(S+0x30,f(d1,d2,*n2,*n1,*offset));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBX,B);u.emu_start(0x4dc103,0x4dc220,count=10000);out=struct.unpack('<3f',u.mem_read(S+0x18,12));v=list(map(float,offset));active=[]
   for n,d in [(n1,d1),(n2,d2)]:
    gate=sum(a*b for a,b in zip(offset,n))>0;active.append(gate)
    if gate:
     dot=sum(a*b for a,b in zip(v,n));proj=[a-dot*b for a,b in zip(v,n)];distance=math.sqrt(sum((a-b)**2 for a,b in zip(v,proj)));sign_crossings+=dot<0;v=[a+b*distance*d/5 for a,b in zip(proj,n)]
   expected=[a+b for a,b in zip(center,v)];assert max(abs(a-b) for a,b in zip(out,expected))<1e-5,(point,n1,n2,out,expected);rows.append(dict(input=point,center=center,first=n1,first_depth=d1,second=n2,second_depth=d2,radius=5,active_from_original_offset=active,output=out))
assert sign_crossings>0
(R/'artifacts/crater-shading-re/shallow-both.json').write_text(json.dumps(dict(exe_sha256=hashlib.sha256((R/'Installed_Game/RF.exe').read_bytes()).hexdigest(),scope=__doc__,sign_crossing_cases=sign_crossings,rows=rows),indent=2));print('PASS:100 original both-limit cases;',sign_crossings,'current-offset sign reversals retain original-offset gate')

# Compare the shared C implementation with the original instruction results.
import subprocess
maximum=0
for row in rows:
 values=[2,row['radius'],*row['center'],*row['input'],*row['first'],row['first_depth'],*row['second'],row['second_depth']]
 actual=list(map(float,subprocess.check_output([str(R/'build/pc/Release/rf_geomod_basis_probe.exe'),'--shallow'],input=' '.join(map(str,values)),text=True).split()))
 error=max(abs(a-b) for a,b in zip(actual,row['output']));maximum=max(maximum,error)
 assert error<1e-5,(row,actual,error)
print('PASS:100 shared C/original comparisons, maximum error',maximum)
