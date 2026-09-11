"""Execute complete original4cd9e0 crossing traversal on a synthetic cube tree."""
import hashlib,itertools,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(original));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im)
b=0x30000000;room=b+0x1000;faces=b+0x2000;vertices=b+0x3000;edges=b+0x4000;nodes=b+0x6000;start=b+0x7000;end=start+12;stack=b+0xe000;stop=b+0xf000;u.mem_map(b,65536)
selected=[]
def hook(m,address,size,context):
 if address==0x4cdc41:selected.append((m.reg_read(UC_X86_REG_ESI)-faces)//0x100)
u.hook_add(UC_HOOK_CODE,hook)
results=[];cases=hits=0
query_specs=globals().get('query_specs');custom=query_specs is not None
if query_specs is None:
 query_specs=[]
 for axis,a,z,flag,cached,tree in itertools.product(range(3),(-3.,-1.,0.,1.,3.),(-3.,-1.,0.,1.,3.),(0,4,8,12,16),(0,1),(0,1)):
  s=[0.]*3;t=[0.]*3;s[axis]=a;t[axis]=z;query_specs.append(dict(start=s,end=t,flags=flag,cached=cached,tree=tree,axis=axis))
for spec in query_specs:
 lo=[-2.]*3;hi=[2.]*3;s=spec['start'];t=spec['end'];flag=spec['flags'];cached=spec['cached'];tree=spec['tree'];axis=spec.get('axis',-1)
 a=s[axis];z=t[axis]
 u.mem_write(b,bytes(0x8000));u.mem_write(b+0x9c,w(1,1,b+0x800));u.mem_write(b+0x800,w(room));u.mem_write(room+0x3c,w(nodes));u.mem_write(start,f(*s,*t))
 for face_index in range(6):
  dim=face_index//2;other=[i for i in range(3) if i!=dim];point=2. if face_index%2 else -2.;plane=[0.,0.,0.,2.];plane[dim]=-1. if face_index%2 else 1.
  verts=[]
  for x,y in ((-2.,-2.),(2.,-2.),(2.,2.),(-2.,2.)):
   v=[0.]*3;v[dim]=point;v[other[0]]=x;v[other[1]]=y;verts.append(v)
  flo=lo[:];fhi=hi[:];flo[dim]=fhi[dim]=point;face=faces+face_index*0x100;edge=edges+face_index*0x100;vert=vertices+face_index*0x100
  u.mem_write(face,f(*plane,*flo,*fhi)+w(flag));u.mem_write(face+0x40,w(edge,room));u.mem_write(face+0x58,w(face+0x100 if (face_index<5 and (not tree or face_index%2==0)) else 0));u.mem_write(vert,f(*[x for row in verts for x in row]))
  for j in range(4):u.mem_write(edge+j*32,w(vert+j*12,0,0,0,0,edge+(j+1)%4*32,edge+(j-1)%4*32))
 for j in range(3):
  node=nodes+j*0x100;u.mem_write(node,f(*lo,*hi)+w(faces+j*0x200));u.mem_write(node+0x20,w(nodes+0x100 if tree and j==0 else 0,nodes+0x200 if tree and j==0 else 0))
 u.mem_write(stack,w(stop,b,room if cached else 0,start,end));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f);selected=[];u.emu_start(0x4cd9e0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 hit=u.reg_read(UC_X86_REG_EAX)&255
 expected=not(flag&12) and a!=z and (min(a,z)<-2<max(a,z) or min(a,z)<2<max(a,z))
 assert len(selected)==hit,(spec,hit,selected)
 if axis>=0:
  assert hit==int(expected),(axis,a,z,flag,cached,tree,hit,selected)
  if hit:assert selected[0]//2==axis
 results.append(dict(axis=axis,start=a,end=z,from_point=s,to_point=t,flags=flag,cached=cached,tree=tree,hit=hit,face=selected[0] if hit else None));hits+=hit;cases+=1
report=dict(result='PASS',cases=cases,hits=hits,original_sha256=digest,scope=('Provided-vector cube traversal with actual geometry callees; original results are the reference for shared comparisons. Boundary and angled segments; no oblique planes or real levels.' if custom else 'Complete unmodified4cd9e0 and all geometry callees, synthetic cube one-node/three-node trees, world-root and supplied-room-root selection, directions/zero movement, flags0/4/8/12/16. Strictly off-face endpoints; boundary/oblique/real-level cases remain. Exact booleans vs analytical crossings and selected-face axis. No shared crossing implementation yet.'),results=results)
(root/'artifacts'/((globals().get('suite','room-crossing'))+'-trace.json')).write_text(json.dumps(report,indent=2)+'\n');print({k:v for k,v in report.items() if k!='results'})
