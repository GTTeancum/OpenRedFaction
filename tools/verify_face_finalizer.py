"""Audit complete original supplied-plane finalization of loaded v180 faces."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);face=base;edges=base+8192;plane=base+20000;verts=base+24576;stack=base+60000;stop=base+64000
levels=json.loads((root/'artifacts/geometry.json').read_text());results=[]
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level',str(root/'Installed_Game'/level['archive']),level['file']]);at=0;cases=0;rejected=[]
 while at<len(raw):
  index,count=struct.unpack_from('<II',raw,at);assert index==cases and 3<=count<=256
  head=raw[at+8:at+48];vertices=raw[at+48:at+48+count*12];at+=108+count*12
  u.mem_write(face,bytes(128));u.mem_write(face+0x40,struct.pack('<I',edges));u.mem_write(plane,head[:16]);u.mem_write(verts,vertices)
  for i in range(count):u.mem_write(edges+i*32,struct.pack('<I',verts+i*12)+bytes(16)+struct.pack('<II',edges+((i+1)%count)*32,edges+((i-1)%count)*32))
  u.mem_write(stack,struct.pack('<III',stop,plane,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,face);u.reg_write(UC_X86_REG_FPCW,0x37f)
  u.emu_start(0x4dfe20,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
  accepted=u.reg_read(UC_X86_REG_EAX)&255;assert accepted in (0,1)
  assert bytes(u.mem_read(face,40))==head,(level['file'],index,'plane/bounds')
  if not accepted:rejected.append(index)
  cases+=1
 assert at==len(raw) and cases==level['faces']
 results.append(dict(file=level['file'],faces=cases,rejected=rejected))
triangle=[(0,0,0),(1,0,0),(0,1,0)]
synthetic=[('triangle',triangle,(0,0,1),1),('opposite normal',triangle,(0,0,-1),1),('perpendicular normal',triangle,(1,0,0),0),('zero normal',triangle,(0,0,0),0),('collinear',[(0,0,0),(1,0,0),(2,0,0)],(0,0,1),0),('one vertex',triangle[:1],(0,0,1),0),('two vertices',triangle[:2],(0,0,1),0),('bow tie',[(0,0,0),(1,1,0),(0,1,0),(1,0,0)],(0,0,1),0),('subnormal area',[(0,0,0),(1e-20,0,0),(0,1e-20,0)],(0,0,1),1),('underflowed area',[(0,0,0),(1e-30,0,0),(0,1e-30,0)],(0,0,1),0)]
for name,points,normal,want in synthetic:
 count=len(points);u.mem_write(face,bytes(128));u.mem_write(face+0x40,struct.pack('<I',edges));u.mem_write(plane,struct.pack('<4f',*normal,0));u.mem_write(verts,struct.pack('<'+str(count*3)+'f',*[v for p in points for v in p]))
 for i in range(count):u.mem_write(edges+i*32,struct.pack('<I',verts+i*12)+bytes(16)+struct.pack('<II',edges+((i+1)%count)*32,edges+((i-1)%count)*32))
 u.mem_write(stack,struct.pack('<III',stop,plane,0));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,face);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4dfe20,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert (u.reg_read(UC_X86_REG_EAX)&255)==want,name
report=dict(result='PASS',levels=len(results),faces=sum(r['faces'] for r in results),rejected=sum(len(r['rejected']) for r in results),synthetic_cases=len(synthetic),scope='Complete unmodified original 4dfe20 with non-null file plane as supplied by the v180 loader. Original plane copy, bounds expansion, triangle-fan area accumulation and acceptance gate run unchanged. Plane/bounds compared to PC adapter. No recomputed-plane branch or mutations.',results=results)
(root/'artifacts/face-finalizer-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
