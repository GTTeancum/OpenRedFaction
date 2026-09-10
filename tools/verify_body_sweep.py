"""Compare ordered body composition against full original499ed0 fixtures."""
import json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
evidence=runpy.run_path(str(root/'tools/inspect_body_sweep_order.py'))
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
identity=f(1,0,0,0,1,0,0,0,1)
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;x.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000;callback=b+0xf100
entry=int(re.search(r'_rf_collision_body_sweep\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
trace=[];heights=[]
def geometry(m,a,size,context):
 sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,request,out,matched=struct.unpack('<5I',m.mem_read(sp,20))
 raw=bytes(m.mem_read(request,44));trace.append(raw)
 solid,sphere,flags,*values=struct.unpack('<3I8f',raw);i=2 if solid==0xffffffff else solid
 z=heights[i];hit=False
 if z is not None:
  fraction=(values[2]-values[6]-z)/(-values[5]);hit=0<=fraction<=values[7]
  if hit:m.mem_write(out,f(fraction,values[0],values[1],z,0,0,1)+w(-1,0,0,i+1))
 m.mem_write(matched,w(hit));m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,geometry,begin=callback,end=callback)
commands=bytearray();expected=[]
for fixture,original in zip(evidence['fixtures'],evidence['results']):
 name,heights,flags,winner=fixture;n=2 if name=='two spheres' else 1
 commands.extend(f(*(0 if z is None else z for z in heights))+w(*(z is not None for z in heights),flags,n))
 raw=bytearray.fromhex(original['output'])
 if winner is not None:struct.pack_into('<I',raw,60,winner+1)
 queries=[]
 for index,q in enumerate(original['queries']):
  queries.append(w(-1 if q['solid']==2 else q['solid'],index%n,q['flags'])+f(*q['start'],*q['delta'],q['radius'],q['limit']))
 expected.append(w(0,winner is not None)+raw+w(len(queries))+b''.join(queries)+bytes((6-len(queries))*44))
 x.mem_write(b,f(0,0,8,0,0,-8)+identity+f(.5)+w(0x460,b+0x1000,n)+f(1))
 x.mem_write(b+0x1000,f(0,0,0,.5,1,0,0,.5))
 for i,z in enumerate(heights[:2]):
  z=0 if z is None else z
  x.mem_write(b+0x2000+i*92,f(-3,-3,z-.0001,3,3,z+.0001,0,0,0)+identity+f(0,0,0)+w(flags if i==0 else 0,100+i))
 x.mem_write(b+0x3000,bytes([0xa5])*68);x.mem_write(b+0x3018,f(1));trace.clear()
 x.mem_write(stack,w(stop,b,b+0x2000,2,callback,0,b+0x3000,b+0x3100));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(b+0x3100,4))+bytes(x.mem_read(b+0x3000,68))
 assert actual==expected[-1][:76],('NXDK result',name,actual.hex(),expected[-1][:76].hex())
 assert trace==queries,('NXDK queries',name)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--body-sweep'],input=commands)
for i,want in enumerate(expected):
 got=actual[i*344:(i+1)*344]
 assert got==want,('PC',evidence['fixtures'][i][0],got.hex(),want.hex())
assert len(actual)==344*len(expected)
report=dict(result='PASS',cases=len(expected),scope='Full original499ed0 with actual plane geometry vs PC composition with reconstructed flat-face geometry; NXDK composition under Unicorn with analytic plane callback. Exact 68-byte normalized response and every query. No XEMU/live scene integration, rotated geometry, cache equivalence or physical response claimed.')
(root/'artifacts/body-sweep-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
