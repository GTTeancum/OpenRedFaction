"""Compare shared PC constructor fields against full original416940 execution."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
cases=[];expected=[]
def observe(g):
 keys=('flags724','flags728','flags814','flags810','flags7c','class_index','num_spheres','keep','seek','kind','source_model','extra_model','emitter_kind')
 values=[g[k] for k in keys]+[struct.unpack('<I',struct.pack('<f',g['life']))[0]]+[int(g[k]) for k in ('replacement','fail','model_fail','emit_found','null')]+[g['case']%4]+g['motion_indices']
 cases.append(w(*values)+g['raw']);read=g['read'];actor=g['actor'];corpse=g['corpse'];success=bool(g['result'])
 out=[0 if success else -3,read(actor+0x7c),read(actor+0x1410),read(0x5caed0),int(success),0]
 if success:
  offsets=(0x20,0x26c,0x2a0,0x1fc,0x2d8,0x2c8,0x2b4,0x2bc,0x2c0,0x2c4,0x2d4,0x78,0x180,0x294,0x29c,0x34,0x80,0x2b8,0x2cc,0x2ac,0x2b0)
  out += [read(corpse+j) for j in offsets]+[0x40a00000,read(corpse+0x2d0),read(corpse+0x2dc),read(corpse+0x2e0),int(bool(read(corpse+0x268))),1,1]+[0]*6
 else:out += [0]*34
 assert len(out)==40;expected.append(w(*out))
runpy.run_path(str(root/'tools/verify_corpse_create_original.py'),init_globals={'observe_case':observe})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-create'],input=b''.join(cases))
assert len(actual)==len(cases)*160
for i,want in enumerate(expected):assert actual[i*160:(i+1)*160]==want,(i,struct.unpack('<40I',actual[i*160:(i+1)*160]),struct.unpack('<40I',want))
import pefile
sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
path=root/'build/xbox/main.exe';p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im)
b=0x30000000;x.mem_map(b,65536);source=b;request=b+0x1000;owner=b+0x2000;head=b+0x3000;count=head+16;result=head+32
spheres=b+0x4000;scratch=b+0x5000;name=b+0x6000;mesh=name+128;emitter=b+0x7000;backend=b+0x8000
callbacks=[b+0x9000+i*16 for i in range(5)];stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_corpse_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
read=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
def put(a,v):x.mem_write(a,w(v))
def string(a):return bytes(x.mem_read(a,128)).split(b'\0',1)[0]
def callback(machine,address,size,unused):
 if address not in callbacks:return
 esp=machine.reg_read(UC_X86_REG_ESP);op=callbacks.index(address);value=0
 if op==0:
  seed=read(esp+12)
  assert read(seed)==77 and read(seed+4)==88 and read(seed+56)==0x40200000
  assert bytes(machine.mem_read(seed+8,48))==bytes(machine.mem_read(request+4,48))
  assert read(seed+64)==v[6] and read(seed+68)==(0x73 if v[0]&0x80000 else 0x33)
  assert bytes(machine.mem_read(read(seed+60),v[6]*24))==wire[92:]
  if not v[15]:
   machine.mem_write(owner,bytes([0xa5])*276)
   for offset,item in ((8,0),(28,0),(176,0x40200000),(208,0x20 if v[14] else 0),(108,0),(116,0x12340008),(124,owner),(212,0)):put(owner+offset,item)
   machine.mem_write(owner+40,bytes(machine.mem_read(seed+8,48)));value=owner
 elif op==1:
  assert string(read(esp+8))==b'corpse.v3c';value=0 if v[16] else 0x12340080
 elif op==2:
  key=string(read(esp+12));value=v[21 if key==b'corpse_drop' else 22 if key==b'corpse_carry' else 20]
 elif op==3:
  effect=read(esp+8);assert read(esp+12)==source and read(esp+16)==owner
  if effect==0:machine.mem_write(owner+216,w(0x12345678,0x23456789))
  if effect==1:put(owner+176,0x40600000)
 elif op==4:value=emitter if v[17] else 0
 machine.reg_write(UC_X86_REG_EAX,value);machine.reg_write(UC_X86_REG_ESP,esp+4);machine.reg_write(UC_X86_REG_EIP,read(esp))
x.hook_add(UC_HOOK_CODE,callback)
def native_case(i,raw,want,existing=()):
 global wire,v
 wire=raw
 v=list(struct.unpack('<23I',wire[:92]));x.mem_write(b,bytes(0x9000))
 x.mem_write(source,w(0x12340007,99,v[4],v[10],v[3],v[2],v[0],v[1],v[5],v[9],mesh if v[14] else 0,
                       77,88,11,123,55,v[11],0x40200000,0x42c80000,0x40a00000,v[13],7,9,v[12],
                       111,112,113,*([0]*42),spheres,v[6]))
 x.mem_write(spheres,wire[92:]);x.mem_write(name,(b'death_forward',b'death_front',b'death_back',b'death_side')[v[19]]+b'\0');x.mem_write(mesh,b'corpse.v3c\0')
 x.mem_write(request,w(name)+struct.pack('<13f',1,2,3,1,0,0,0,1,0,0,0,1,1000)+w(1000,v[7]|(v[8]<<8),scratch,4))
 x.mem_write(head,w(head,head));x.mem_write(backend,w(*callbacks,0))
 previous=head
 for j,(flags,bits,created,fade) in enumerate(existing):
  node=b+0xa000+j*276;link=node+92;x.mem_write(node,bytes(276));put(node+8,flags);put(node+12,bits);put(node+180,created);put(node+4,fade)
  x.mem_write(link,w(head,previous));put(previous,link);put(head+4,link);previous=link
 put(count,len(existing))
 x.mem_write(stack,w(stop,0 if v[18] else source,request,head,count,backend,result));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==stop
 success=bool(read(result));got=[x.reg_read(UC_X86_REG_EAX),read(source+8),read(source+64),read(count),int(success),0]
 if success:
  offsets=(128,132,136,140,144,148,152,156,160,164,168,172,176,180,12,0,28,32,36,16,20,24,112,216,220)
  got += [read(owner+j) for j in offsets]+[int(bool(read(owner+108))),int(read(head)==(b+0xa000+92 if existing else owner+92) and read(head+4)==owner+92 and read(owner+92)==head and read(owner+96)==previous),int(read(owner+88)==owner)]+[read(owner+j) for j in range(184,208,4)]
 else:got += [0]*34
 assert w(*got)==want,('NXDK',i,got,struct.unpack('<40I',want))
 return w(*[read(b+0xa000+j*276+offset) for j in range(len(existing)) for offset in (12,4)],read(owner+4) if success else 0)
for i,(wire,want) in enumerate(zip(cases,expected)):native_case(i,wire,want)
report=dict(result='PASS',cases=len(cases),nxdk_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),scope='Shared PC/NXDK constructor fields/source ownership/physics-seed copy vs complete original416940. Supplied allocation/resource backends. No multi-corpse constructor retention, full resource trace comparison or live scene dispatch yet.')
(root/'artifacts/corpse-create-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
