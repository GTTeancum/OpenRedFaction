"""Shared room-crossing traversal vs full original cube traversal."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
ev=runpy.run_path(str(root/'tools/verify_room_crossing_trace.py'));w=ev['w'];f=ev['f']
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);b=0x30000000;x.mem_map(b,65536);stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_collision_cross_rooms\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=[];expected=[]
for row in ev['results']:
 start=[0.]*3;end=[0.]*3;start[row['axis']]=row['start'];end[row['axis']]=row['end'];commands.append(f(*start,*end)+w(row['flags'],row['cached'],row['tree']));expected.append(w(0,0 if row['hit'] else -1,row['face'] if row['hit'] else -1))
for offset in (0,12):wire=bytearray(commands[0]);wire[offset:offset+4]=w(0x7fc00000);commands.append(bytes(wire));expected.append(w(-2,0xa5a5a5a5,0xa5a5a5a5))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--cross-rooms'],input=b''.join(commands));assert actual==b''.join(expected),('PC',next(i for i,(a,c) in enumerate(zip(actual,b''.join(expected))) if a!=c))
faces=b+0x1000;verts=b+0x2000;nodes=b+0x3000;tree=b+0x4000;room=b+0x4100;roots=b+0x4200;scratch=b+0x4300;points=b+0x4400;out=b+0x4500
for case,(wire,want) in enumerate(zip(commands,expected)):
 flag,cached,branched=struct.unpack('<3I',wire[24:]);x.mem_write(points,wire[:24])
 for index in range(6):
  axis=index//2;other=[j for j in range(3) if j!=axis];plane=[0.,0.,0.,2.];plane[axis]=-1. if index%2 else 1.;lo=[-2.]*3;hi=[2.]*3;lo[axis]=hi[axis]=2. if index%2 else -2.;polygon=[]
  for a,c in ((-2.,-2.),(2.,-2.),(2.,2.),(-2.,2.)):
   point=[0.]*3;point[axis]=lo[axis];point[other[0]]=a;point[other[1]]=c;polygon.extend(point)
  x.mem_write(verts+index*48,f(*polygon));x.mem_write(faces+index*72,f(*plane,*lo,*hi)+w(verts+index*48,4,0,flag,0,0,0,0))
 for index in range(3):x.mem_write(nodes+index*40,f(-2,-2,-2,2,2,2)+w(index*2,2 if branched else 6,1 if branched and index==0 else -1,2 if branched and index==0 else -1))
 x.mem_write(tree,w(0,nodes,faces,0,scratch,3 if branched else 1,6,3,0,0));x.mem_write(room,bytes(36)+w(tree));x.mem_write(roots,w(0));x.mem_write(out,bytes([0xa5])*8)
 x.mem_write(stack,w(stop,room,1,roots,1,0 if cached else -1,points,points+12,out));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,8));assert got==want,('NXDK',case,got.hex(),want.hex())
report=dict(result='PASS',original_cases=1500,port_guards=2,original_sha256=ev['digest'],nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Shared complete room crossing vs original1500 cube cases, exact first room/face selection on PC and actual NXDK. Two nonfinite guards preserve output. One world root, one/three-node trees; boundary, oblique, multiple roots and real levels not yet covered.')
(root/'artifacts/room-crossing.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
