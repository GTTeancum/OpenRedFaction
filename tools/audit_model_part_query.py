"""Original54e000 traversal/preparation versus PC/NXDK; only54daa0 supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x1000;count=model+0x48;query=b+0x2000;hit=b+0x3000;backend=b+0x4000;callback=b+0x5000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe)
model=b+0x1000;part=b+0x1100;table=b+0x1300;query=b+0x2000;hit=b+0x3000
u.mem_write(0x1754424,b'\x1f');u.mem_write(0x1754525,b'\x03')
u.mem_write(model+0x4c,w(part));u.mem_write(part+0x8c,w(table))
triangles=[]
def observe(m,address,size,_):
    if address==0x54dcd0:
        sp=m.reg_read(UC_X86_REG_ESP);triangles.append(struct.unpack('<6I',m.mem_read(sp+4,24)))
u.hook_add(UC_HOOK_CODE,observe,None,0x54dcd0,0x54dcd0)
cases=0;hits=0;fallbacks=0;empty=0
for flags in (0,1,2,3):
 for reset in (0,1,0x101,2):
  for fallback in (0,1):
   for radius in (0,.25):
    for vacant in (0,1):
     offset=[3.,-2.,5.];origin=[7.,11.,-3.];matrix=[1.,0.,0.,0.,1.,0.,0.,0.,1.]
     local=[3.,-2.,7.];start=local if flags&2 else [local[i]+origin[i] for i in range(3)]
     q=f(origin+matrix+start+[0.,0.,-4.,radius])+w(flags)+b'\xa5'*24
     initial=f([.1,11,12,13,14,15,16])+w(0x12345678)
     u.mem_write(table,w(2,b+0x4000,b+0x4100));u.mem_write(table+0x1c,f(offset));u.mem_write(table+0x2c,f([-3,-3,-3,3,3,3]))
     for n in range(2):
      lod=b+0x4000+n*0x100;batch=b+0x5000+n*0x100;verts=b+0x6000+n*0x100;plane=b+0x7000+n*0x100;records=b+0x8000+n*0x100
      u.mem_write(lod,b'\x00'*0x44);u.mem_write(lod+8,w(batch));u.mem_write(lod+12,struct.pack('<H',0 if vacant else 1));u.mem_write(lod+0x40,w(0x10 if n==1 and fallback else 0))
      u.mem_write(batch,b'\x00'*0x38);u.mem_write(batch+4,w(verts));u.mem_write(batch+0x10,w(plane,records));u.mem_write(batch+0x2a,struct.pack('<H',1))
      u.mem_write(verts,f([-2,-2,n,2,-2,n,0,2,n]));u.mem_write(plane,f([0,0,1,-n]));u.mem_write(records,struct.pack('<4H',0,1,2,0x20))
     u.mem_write(query,q);u.mem_write(hit,initial);triangles.clear()
     u.mem_write(stack,w(stop,0,query,hit,reset));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,model)
     u.emu_start(0x54daa0,stop,count=100000)
     assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+20
     selected=0 if fallback else 1;time=(2-selected-radius)/4;limit=1 if reset&255==1 else struct.unpack('<f',initial[:4])[0]
     accepted=int(not vacant and time<limit);actual=u.reg_read(UC_X86_REG_EAX)&255;assert actual==accepted
     want=bytearray(initial)
     if reset&255==1:want[:4]=f([1]);want[28:]=w(0)
     if accepted:want=f([time,3,-2,5+selected,0,0,1])+w(b+0x8000+selected*0x100)
     assert bytes(u.mem_read(hit,32))==want,(flags,reset,fallback,radius,vacant)
     assert bytes(u.mem_read(query,80))==q[:80]
     assert bytes(u.mem_read(query+80,24))==f([0,0,2,0,0,-4])
     assert len(triangles)==(0 if vacant else 1)
     if triangles:assert triangles[0]==(b+0x4000+selected*0x100,b+0x5000+selected*0x100,0,query,hit,0x20)
     cases+=1;hits+=accepted;fallbacks+=fallback;empty+=vacant
report=dict(result='PASS',cases=cases,hits=hits,lod_fallback_cases=fallbacks,empty_cases=empty,original_sha256=sha,scope='Complete original54daa0 with actual thin/sphere triangle callees and read-only dispatch observation. Analytic identity-transform fixtures verify LOD fallback, shared part metadata, local scratch, reset low byte, hit translation, preserved misses and empty batches. Not a reconstructed part implementation or arbitrary rotation/multi-batch proof.')
(root/'artifacts/model-part-audit.json').write_text(json.dumps(report,indent=2));print(report)
