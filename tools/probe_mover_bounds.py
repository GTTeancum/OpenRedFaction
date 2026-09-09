"""Execute original mover AABB/sphere finalization and creation-radius block."""
import hashlib,json,math,random,struct,sys
from pathlib import Path
import pefile
from inspect_geometry import inspect
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EDI,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x400000);solid=base;array=base+4096;verts=base+0x100000;stack=base+0x300000;stop=stack+4096
f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
def call(addr):
 u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,solid);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(addr,stop,count=3000000);assert u.reg_read(UC_X86_REG_EIP)==stop

def measure(points):
 n=len(points);assert n*4<0xff000 and n*12<0x200000
 initial=bytearray(bytes([0xa5])*256);struct.pack_into('<3I',initial,0x78,n,n,array);u.mem_write(solid,bytes(initial));u.mem_write(array,struct.pack('<'+'I'*n,*[verts+i*12 for i in range(n)]));u.mem_write(verts,b''.join(struct.pack('<3f',*p) for p in points))
 call(0x4cf9a0);raw=bytes(u.mem_read(solid,256))
 if not n:
  assert raw==initial # Parent routine skips even sphere clear on empty vertices.
  call(0x4cf500);wanted=bytearray(initial);wanted[0x60:0x70]=bytes(16);assert bytes(u.mem_read(solid,256))==wanted;return None
 epsilon=f32(.0001);bounds=struct.pack('<6f',*[f32(min(p[j] for p in points)-epsilon) for j in range(3)],*[f32(max(p[j] for p in points)+epsilon) for j in range(3)])
 assert raw[0x48:0x60]==bounds
 wanted=bytearray(initial);wanted[0x48:0x70]=raw[0x48:0x70];assert raw==wanted
 radius,*center=struct.unpack('<4f',raw[0x60:0x70]);assert radius>=0 and all(math.isfinite(x) for x in [radius,*center])
 # Float-rounding tolerance for a containment invariant, not an exact port comparison.
 excess=max(math.dist(p,center)-radius for p in points);assert excess<=max(1e-5,4e-6*max(radius,*map(abs,center))),excess
 u.mem_write(stack,bytes(512));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,solid);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(0x46b075,0x46b098,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x46b098
 origin_radius,=struct.unpack('<f',u.mem_read(stack+0x98,4));assert abs(origin_radius-(math.sqrt(sum(c*c for c in center))+radius))<=max(1e-6,origin_radius*2e-7)
 return dict(bounds=list(struct.unpack('<6f',bounds)),center=center,radius=radius,origin_radius=origin_radius,original_bytes=raw[0x48:0x70].hex()+bytes(u.mem_read(stack+0x98,4)).hex(),containment_excess=excess)
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());movers=json.loads((root/'artifacts/movers.json').read_text());results=[]
for level in movers['results']:
 directory=next(l for l in levels if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in directory['sections'] if s['type']=='0x2000');archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 records=[]
 for record in level['records']:
  geo=data[record['geometry_offset']:record['geometry_offset']+record['geometry_bytes']];info=inspect(geo,True);at=info['vertices_offset'];points=list(struct.iter_unpack('<3f',geo[at:at+12*info['vertices']]));value=measure(points);assert value is not None;records.append(dict(uid=record['uid'],vertices=len(points),**value))
 results.append(dict(file=level['file'],archive=level['archive'],records=records))
rng=random.Random(0x4cf500);synthetic=[[],[(0,0,0)],[(1,2,3)],[(1,2,3)]*4,[(0,0,0),(1,0,0),(0,1,0),(0,0,1)]]
for i in range(500):synthetic.append([tuple(f32(rng.uniform(-100,100)) for _ in range(3)) for _ in range(rng.randrange(1,65))])
changed=0
for points in synthetic:
 a=measure(points);b=measure(points[::-1]);changed+=a!=b
report=dict(result='PASS',levels=len(results),movers=sum(len(l['records']) for l in results),synthetic_calls=len(synthetic)*2,order_sensitive_sets=changed,scope='Complete unmodified 4cf9a0 and its sphere callee with original vertex-list access; exact AABB expansion and untouched-object checks, tolerance-based containment invariant; original 46b075..46b098 creation-radius block. Exact original bytes recorded as evidence, not runtime data. No C/NXDK radius port or full object creation.',results=results)
(root/'artifacts/mover-bounds-original.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
