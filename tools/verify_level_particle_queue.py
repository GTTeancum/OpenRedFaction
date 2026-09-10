"""Original particle/emitter collection, owner transform and append versus PC/NXDK."""
import runpy,struct,re,random,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
symbols=(root/'build/xbox/main.map').read_text()
def sym(n):return int(re.search('_'+n+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
def invoke(m,address,args):
 m.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=5000000);assert m.reg_read(UC_X86_REG_EIP)==stop
 return m.reg_read(UC_X86_REG_EAX)
for m in (u,x):m.mem_map(base+0x10000,0x200000)
state=base+0x100000;queue=base+0x180000;original_particles=base+0x10000;original_emitters=base+0x20000;parent_address=base+0x30000
parent=bytes(84);found=0;lookups=[[],[]]
def hook(m,address,size,data):
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if m is u:
  handle=struct.unpack('<I',m.mem_read(sp+4,4))[0];lookups[0].append(handle);m.reg_write(UC_X86_REG_EAX,parent_address if handle and found else 0)
 else:
  context,handle,out=struct.unpack('<3I',m.mem_read(sp+4,12));lookups[1].append(handle)
  m.mem_write(out,parent if handle else bytes(72)+struct.pack('<iII',-1,0,0));m.reg_write(UC_X86_REG_EAX,0)
 m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook,begin=0x40a0e0,end=0x40a0e0);x.hook_add(UC_HOOK_CODE,hook,begin=base+0xf100,end=base+0xf100)
rng=random.Random(0x497c20);commands=bytearray();results=bytearray();appends=0
initial=bytes([0xa5])*(2048*48);links=(3,129,0,1)
def room_address(room):return base+0x40000+room*512 if room else 0
for case in range(128):
 room=case%5;count=(0,1,2047,2048)[(case//5)%4];frustum=bytearray(156)
 planes=(case//4)%7;struct.pack_into('<I',frustum,144,planes);u.mem_write(0x1818b8c,struct.pack('<I',planes))
 for i in range(6):
  normal=[0.,0.,0.];normal[i//2]=(-1.,1.)[i%2]
  plane=struct.pack('<4fI',*normal,-8.,0);frustum[i*20:(i+1)*20]=plane;u.mem_write(0x1818a6c+i*28,plane+bytes(8))
 found=case%2;basis=(0,0,1,0,1,0,-1,0,0);pos=(2.,-3.,4.)
 parent=struct.pack('<15fIfIiII',*pos,*basis,0,0,0,0x10001,1.,0,123,2,found)
 u.mem_write(parent_address,bytes(512));u.mem_write(parent_address+0x3c,struct.pack('<3f',*pos));u.mem_write(parent_address+0x48,struct.pack('<9f',*basis))
 particles=bytearray();emitters=bytearray()
 x.mem_write(state,bytes(222300));invoke(x,sym('rf_particle_pool_init'),(state+222248,state,state+221184,133));invoke(x,sym('rf_emitter_pool_init'),(state+222268,state+192000,state+222248))
 for i in range(8):
  p=bytearray(120);position=tuple(rng.randint(-48,48)/4 for _ in range(3));particle_room=(i+case)%5
  struct.pack_into('<3f',p,12,*position);struct.pack_into('<f',p,56,(-1,.5,3)[i%3]);struct.pack_into('<I',p,100,particle_room);particles.extend(p)
  list_id=(2,4,3)[i%3];n=i+3 if i+3<8 else 1600+list_id
  struct.pack_into('<I',p,0,n);x.mem_write(state+i*120,bytes(p))
  struct.pack_into('<I',p,0,original_particles+(i+3)*120 if i+3<8 else {2:0x7a3b80,4:0x7bd670,3:0x7a3c7c}[list_id]);struct.pack_into('<I',p,100,room_address(particle_room));u.mem_write(original_particles+i*120,bytes(p))
 for list_id,index in ((2,0),(4,1),(3,2)):
  x.mem_write(state+221184+list_id*8,struct.pack('<I',index));u.mem_write({2:0x7a3b80,4:0x7bd670,3:0x7a3c7c}[list_id],struct.pack('<I',original_particles+index*120))
 for i in range(4):
  e=bytearray(228);owner=(-1,0,0x10001)[(i+case)%3];er=(i+case)%5;flags=0x40 if (i+case)%2 else 0
  position=tuple(rng.randint(-48,48)/4 for _ in range(3));radius=(-1.,1.,4.)[i%3]
  struct.pack_into('<i6f',e,0,owner,*position,0,0,1);struct.pack_into('<I',e,52,flags);struct.pack_into('<I',e,72,er);struct.pack_into('<f',e,204,radius);emitters.extend(e)
  struct.pack_into('<I',e,216,links[i]);x.mem_write(state+192000+i*228,bytes(e))
  raw=bytearray(0x160);struct.pack_into('<i6f',raw,4,owner,*position,0,0,1);struct.pack_into('<I',raw,0x38,flags);struct.pack_into('<I',raw,0x4c,room_address(er));struct.pack_into('<f',raw,0x9c,radius);struct.pack_into('<I',raw,0x148,original_emitters+links[i]*0x160 if links[i]!=129 else 0x7bd6e8);u.mem_write(original_emitters+i*0x160,bytes(raw))
 x.mem_write(state+222268+16,struct.pack('<I',2));u.mem_write(0x7bd830,struct.pack('<I',original_emitters+2*0x160))
 u.mem_write(0x88fd1c,b'\0');u.mem_write(0x9bb56c,bytes(4));u.mem_write(0x87bb00,bytes(12));u.mem_write(0x9bb550,struct.pack('<I',count));u.mem_write(0x88fd20,initial)
 lookups=[[],[]];invoke(u,0x4967a0,(room_address(room),));invoke(u,0x497c20,(room_address(room),))
 after=struct.unpack('<I',u.mem_read(0x9bb550,4))[0];out=bytearray(u.mem_read(0x88fd20,len(initial)))
 for i in range(count,after):
  obj=struct.unpack_from('<I',out,i*48)[0];callback=struct.unpack_from('<I',out,i*48+44)[0]
  if callback==0x496830:index=(obj-original_particles)//120;kind=1
  else:assert callback==0x497bf0;index=(obj-original_emitters)//0x160;kind=2
  struct.pack_into('<I',out,i*48,index);struct.pack_into('<I',out,i*48+44,kind)
 result=struct.pack('<II',0,after)+out;results.extend(result);appends+=after-count
 commands.extend(bytes(frustum)+struct.pack('<II',room,count)+particles+emitters+parent)
 x.mem_write(base,bytes(frustum));x.mem_write(base+0x500,struct.pack('<I',state)+bytes(28));x.mem_write(base+0x600,struct.pack('<I',count));x.mem_write(queue,initial)
 before_state=bytes(x.mem_read(state,222300))
 status=invoke(x,sym('rf_level_particles_queue_room'),(base+0x500,room,base,base+0xf100,0,queue,2048,base+0x600))
 actual=struct.pack('<I',status)+bytes(x.mem_read(base+0x600,4))+bytes(x.mem_read(queue,len(initial)))
 assert actual==result,(case,[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:15])
 assert before_state==bytes(x.mem_read(state,222300)) and lookups[0]==lookups[1],(case,lookups)
actual=subprocess.check_output([str(c['probe']),'--level-particle-queue'],input=commands)
assert actual==results,[(i,a,b) for i,(a,b) in enumerate(zip(actual,results)) if a!=b][:15]
report=dict(result='PASS',cases=128,appends=appends,scope='Full original 4967a0 and 497c20, actual 496bc0 owner transform, 4d3560 and sphere rejection. Only registry lookup resolves supplied parent. Exact normalized queue bytes/counts on PC/NXDK, lookup order and unchanged native source state. Mixed rooms, active-list order, skipped global pool 1, disabled/empty emitters, owner flags, full queues. No GPU or live scene wiring.')
(root/'artifacts/level-particle-queue-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
