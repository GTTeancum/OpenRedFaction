"""C/NXDK vertex bounds vs complete original finalization and creation radius."""
import json,runpy,struct,subprocess,re,math,hashlib
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
original=runpy.run_path(str(root/'tools/probe_mover_bounds.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_FPSW
cases=[];expected=[]
def add(points,value):
 cases.append(struct.pack('<I',len(points))+b''.join(struct.pack('<3f',*p) for p in points));expected.append(struct.pack('<i',0)+bytes.fromhex(value['original_bytes']) if value else struct.pack('<i',-3)+bytes([0xa5])*44)
for level in original['results']:
 directory=next(l for l in original['levels'] if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in directory['sections'] if s['type']=='0x2000');archive=next(a for a in original['inventory']['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 layout=next(l for l in original['movers']['results'] if l['file']==level['file'] and l['archive']==level['archive'])
 with (root/'Installed_Game'/level['archive']).open('rb') as f:f.seek(entry['offset']+section['offset']+8);data=f.read(section['size'])
 for record,value in zip(layout['records'],level['records']):
  geo=data[record['geometry_offset']:record['geometry_offset']+record['geometry_bytes']];info=original['inspect'](geo,True);at=info['vertices_offset'];points=list(struct.iter_unpack('<3f',geo[at:at+12*info['vertices']]));add(points,value)
for points in original['synthetic']:
 for p in [points,points[::-1]]:add(p,original['measure'](p))
for p in [[(math.nan,0,0)],[(0,math.inf,0)],[(0,0,-math.inf)],[(3e38,0,0),(-3e38,0,0)]]:
 cases.append(struct.pack('<I',len(p))+b''.join(struct.pack('<3f',*v) for v in p));expected.append(struct.pack('<i',-2)+bytes([0xa5])*44)
raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--vertex-bounds'],input=b''.join(cases));assert len(raw)==48*len(cases)
for i,want in enumerate(expected):assert raw[i*48:i*48+48]==want,('PC',i,raw[i*48:i*48+48].hex(),want.hex())
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(xb,(len(im)+4095)//4096*4096);u.mem_write(xb,im);base=0x30000000;u.mem_map(base,0x400000);out=base+0x200000;stack=base+0x300000;stop=stack+4096
entry=int(re.search(r'_rf_collision_vertex_bounds\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(wire,want) in enumerate(zip(cases,expected)):
 count,=struct.unpack_from('<I',wire);u.mem_write(base,wire[4:] or bytes(12));u.mem_write(out,bytes([0xa5])*44);u.mem_write(stack,struct.pack('<4I',stop,base,count,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(entry,stop,count=3000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',u.reg_read(UC_X86_REG_EAX))+bytes(u.mem_read(out,44));assert got==want,('NXDK',i,got.hex(),want.hex());assert (u.reg_read(UC_X86_REG_FPSW)>>11)&7==0
report=dict(result='PASS',installed_movers=1406,synthetic_cases=1010,port_guards=4,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Exact AABB, sphere radius/center and origin radius vs complete original 4cf9a0/4cf500 and creator arithmetic block; all installed mover vertex arrays plus forward/reversed synthetic sets. PC and NXDK (Unicorn), untouched failure outputs and balanced x87 stack. No runtime object binding or XEMU.')
(root/'artifacts/vertex-bounds-verification.json').write_text(json.dumps(report,indent=2));print(report)
