"""Owned mover collision faces vs installed bytes and original finalizer/list append."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile
from inspect_geometry import inspect
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,1024*1024);face=base;edges=base+8192;plane=base+20000;verts=base+24576;stack=base+60000;stop=base+64000;lst=base+65536;nodes=base+66000
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());movers=json.loads((root/'artifacts/movers.json').read_text());results=[]
def execute(addr,ecx,args):
 u.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,ecx);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(addr,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop
for level in movers['results']:
 directory=next(l for l in levels if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in directory['sections'] if s['type']=='0x2000');archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-faces',str(root/'Installed_Game'/level['archive']),level['file']]);count,peak=struct.unpack_from('<2I',raw);assert count==level['count'];at=8;total=0
 for record in level['records']:
  geo=data[record['geometry_offset']:record['geometry_offset']+record['geometry_bytes']];info=inspect(geo,True);vo=info['vertices_offset'];fa=vo+info['vertices']*12+4
  n,=struct.unpack_from('<I',raw,at);at+=4;assert n==record['faces'];u.mem_write(lst,bytes(8))
  for index in range(n):
   idx,corners=struct.unpack_from('<2I',raw,at);head=raw[at+8:at+48];metadata=raw[at+48:at+72];vertices=raw[at+72:at+72+corners*12];at+=72+corners*12
   disk=geo[fa:fa+56];assert idx==index and corners==struct.unpack_from('<I',disk,52)[0] and head[:16]==disk[:16];assert corners<=256
   portal=struct.unpack_from('<h',disk,36)[0];flags=struct.unpack_from('<I',disk,40)[0];assert metadata==struct.pack('<IIiIII',0,flags,portal,0,0,0)
   stride=12 if struct.unpack_from('<I',disk,20)[0]==0xffffffff else 20
   wanted=b''.join(geo[vo+struct.unpack_from('<I',geo,fa+56+j*stride)[0]*12:vo+struct.unpack_from('<I',geo,fa+56+j*stride)[0]*12+12] for j in range(corners));assert vertices==wanted;fa+=56+corners*stride
   u.mem_write(face,bytes(128));u.mem_write(face+0x40,struct.pack('<I',edges));u.mem_write(plane,head[:16]);u.mem_write(verts,vertices)
   for j in range(corners):u.mem_write(edges+j*32,struct.pack('<I',verts+j*12)+bytes(16)+struct.pack('<II',edges+((j+1)%corners)*32,edges+((j-1)%corners)*32))
   execute(0x4dfe20,face,[plane,0]);assert u.reg_read(UC_X86_REG_EAX)&255==1,(level['file'],record['uid'],index,'rejected');assert bytes(u.mem_read(face,40))==head
   node=nodes+index*128;u.mem_write(node,bytes([0xa5])*128);execute(0x4d3160,lst,[node]);total+=1
  current,actual_count=struct.unpack('<2I',u.mem_read(lst,8));assert actual_count==n
  for j in range(n):assert current==nodes+j*128;current,=struct.unpack('<I',u.mem_read(current+0x54,4))
  assert current==0
 assert at==len(raw);results.append(dict(file=level['file'],movers=count,faces=total,peak_single_flat=peak))
report=dict(result='PASS',levels=len(results),movers=sum(r['movers'] for r in results),faces=sum(r['faces'] for r in results),max_single_flat=max(r['peak_single_flat'] for r in results),scope='PC owned flat storage matches file plane/corner/initial filter bytes after source closure, exact and short budgets; all faces accepted by complete original supplied-plane finalizer with exact bounds; original append helper preserves order. No original full loader, moving-object creation or XEMU execution.',results=results)
(root/'artifacts/mover-flat-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
