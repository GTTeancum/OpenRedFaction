"""Execute original499670 and all vector/ray helpers without hooks."""
import hashlib,json,struct,sys,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
native=None
if '--nxdk' in sys.argv:
 pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));ni=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
 native=Uc(UC_ARCH_X86,UC_MODE_32);native.mem_map(origin,(len(ni)+4095)&~4095);native.mem_write(origin,ni);native.mem_map(b,65536)
 entry=int(re.search(r'_rf_collision_actors_ground_spheres\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
rows=[]
for pose in range(12):
 for radius in [.25,.4,.4001,.5,.75,1]:
  for offset in [0,.25,.49,.75,1.25]:
   for limit in [0,.25,1]:
    origin=(0,0,0) if pose==0 else (2,-1,3)
    basis_index=pose%4;count=1 if pose<4 else 2;start_y=3 if pose<8 else .25
    matrices=[(1,0,0,0,1,0,0,0,1),(0,1,0,-1,0,0,0,0,1),(.6,0,.8,0,1,0,-.8,0,.6),(.36,.48,.8,-.8,.6,0,-.48,-.64,.6)]
    u.mem_write(b,bytes(0x6000));u.mem_write(b,f(offset+origin[0],start_y+origin[1],origin[2]));u.mem_write(b+16,f(offset+origin[0],-start_y+origin[1],origin[2]))
    for body,spheres,r in [(b+0x1000,b+0x3000,.1),(b+0x2000,b+0x4000,radius)]:
     matrix=matrices[basis_index if body==b+0x1000 else (-basis_index)%4]
     center=(0,0,0) if pose==0 else ((.2,.1,-.1) if body==b+0x1000 else (-.1,.25,.1))
     u.mem_write(body+0x74,f(*matrix));u.mem_write(body+0xfc,w(count,count,spheres));u.mem_write(spheres,f(*center,r)+bytes(8))
     if count==2:u.mem_write(spheres+24,f(center[0]+.25,center[1]-.2,center[2]+.1,r*.75)+bytes(8))
     if body==b+0x2000:u.mem_write(body+0x5c,f(*origin))
    out=b+0x5000;packet=bytearray(b'\xa5'*68);packet[24:28]=f(limit);u.mem_write(out,bytes(packet));before=bytes(u.mem_read(b,0x5000))
    u.mem_write(stack,w(stop,b,b+16,b+0x1000,b+0x2000,out));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x499670,stop,count=50000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and before==bytes(u.mem_read(b,0x5000))
    result=bytes(u.mem_read(out,68));hit=bool(u.reg_read(UC_X86_REG_EAX)&255)
    if pose==0 and offset==0 and limit==1:
     assert hit==(radius>.4),(radius,hit)
     if hit:assert abs(struct.unpack_from('<f',result,24)[0]-(3-(radius+.1))/6)<1e-6
    if limit==0:assert not hit
    if native:
     native.mem_write(b,bytes(u.mem_read(b,0x6000)));native.mem_write(out,bytes(packet))
     for actor,spheres in [(b+0x6000,b+0x3000),(b+0x7000,b+0x4000)]:
      native.mem_write(actor,bytes(232));native.mem_write(actor+76,w(count,spheres));native.mem_write(actor+152,bytes(u.mem_read((b+0x1000 if actor==b+0x6000 else b+0x2000)+0x74,36)))
      if actor==b+0x7000:native.mem_write(actor+24,f(*origin))
     native.mem_write(stack,w(stop,b,b+16,b+0x6000,b+0x7000,out));native.reg_write(UC_X86_REG_ESP,stack);native.reg_write(UC_X86_REG_FPCW,0x27f)
     native.emu_start(entry,stop,count=50000);assert native.reg_read(UC_X86_REG_EIP)==stop
     actual=bytes(native.mem_read(out,68));assert actual==result and bool(native.reg_read(UC_X86_REG_EAX)&255)==hit,(radius,offset,limit,hit,actual.hex(),result.hex())
    rows.append(dict(pose=pose,radius=radius,offset=offset,limit=limit,hit=hit,point_normal_time=list(struct.unpack('<7f',result[:28])),changed=[i for i in range(68) if result[i]!=packet[i]]))
report=dict(result='PASS',original_sha256=sha,nxdk_compared=bool(native),cases=rows,scope='Full499670, translated/rotated poses, one/two spheres and initial-overlap cases, vertical sweep. No hooks or original game launch. NXDK comparison is optional and explicitly reported.')
(ROOT/'artifacts/geomod-postedit-re/ground-sphere.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(rows),'original ground sphere cases;',sum(r['hit'] for r in rows),'hits')
