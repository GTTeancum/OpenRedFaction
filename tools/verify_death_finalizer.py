"""Compare full ordinary-SP finalizer orchestration: original, PC and NXDK."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
fb=lambda v:struct.unpack('<I',f(v))[0]
inputs=[];expected=[]
def observe(g):
 keys=('kind','special','has_parent','has_player','explosion','skip','action','replacement','in_region','mode','hit','resolved')
 raw=[int(g[k]) for k in keys]+[fb(g['normal_y']),int(g['face']),fb(g['area'])]+[int(g[k]) for k in ('fail_create','burn','drop','mutate','initial_flags')]
 inputs.append(w(*raw)+f(*g['normal'],*g['initial_basis']));u=g['u'];a=g['actor'];c=g['corpse'];p=g['player'];read=g['read']
 out=w(0,read(a+0x7c),read(a+0x810),read(a+0x824),read(a+0x13d8),read(c+0x2d0),read(p+0x14),read(p+0xfb0),read(g['child']+0x34))+bytes(u.mem_read(a+0x48,36))+bytes(u.mem_read(a+0x1b4,68))
 trace=[]
 for event in g['trace']:
  key=event[0]
  if key=='damage':row=(4 if event[1]==102 else 5,event[1],0,0)
  elif key=='parent_detach':row=(6,100,0,0)
  elif key=='child_detach':row=(7,event[1],1,event[2])
  elif key=='player_lookup':row=(8,100,0,0)
  elif key=='explosion':row=(10,100,0,0)
  elif key=='drop':row=(11,200,0,event[1])
  elif key=='retarget':row=(12,event[1],event[2],0)
  elif key=='release':row=(13,event[1],0,0)
  elif key=='tail_predicate':row=(14,100,0,0)
  elif key=='create':row=(16,int(bool(event[1])),0,0)
  elif key=='probe':row=(17,0,0,0)
  elif key=='area':row=(18,0x1234,0,0)
  elif key=='region':row=(19,0,0,0)
  else:raise AssertionError(event)
  trace.extend(row)
 assert len(trace)<=128;expected.append(out+w(len(trace)//4,*trace,*([0]*(128-len(trace)))))
runpy.run_path(str(root/'tools/inspect_death_finalizer.py'),init_globals={'observe_case':observe})
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--finalize-sp'],input=b''.join(inputs))
assert len(pc)==len(inputs)*656
for i,want in enumerate(expected):
 got=pc[i*656:(i+1)*656];assert got==want,('PC',i,[(j,struct.unpack_from('<I',got,j)[0],struct.unpack_from('<I',want,j)[0]) for j in range(0,656,4) if got[j:j+4]!=want[j:j+4]])
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32)
x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
b=0x30000000;x.mem_map(b,0x20000);source=b;parent=b+0x1000;child=b+0x1100;corpse=b+0x2000;player=b+0x3000;backend=b+0x4000;names=b+0x5000;stub=b+0x6000;floatret=b+0x7000;stack=b+0x1e000;stop=b+0x1f000
entry=int(re.search(r'\s_rf_entity_finalize_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
readx=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
put=lambda a,*v:x.mem_write(a,w(*v))
trace=[]
def log(op,a=0,bb=0,v=0):trace.extend((op,a,bb,v))
def hook(cpu,address,size,data):
 if address<stub or address>=stub+7*16 or (address-stub)%16:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:readx(sp+4+4*i);op=(address-stub)//16;ret=0
 assert arg(0)==0x99
 if op==0:
  kind,a,bb=arg(1),arg(2),arg(3);v=0
  if kind in (0,2):ret=1
  elif kind in (1,3):ret=102
  elif kind==15:ret=raw[11]
  elif kind==9:put(player,-1,(readx(player+4)&0xffffff00))
  else:
   if kind==7:v=readx(child+12)
   if kind==11:
    v=readx(source+12)
    if raw[18]:put(source+12,v|0x800)
   if kind==12 and raw[18]:put(source+28,readx(source+28)+1)
   if kind==8 and raw[3]:ret=77
   log(kind,a,bb,v)
 elif op==1:ret=parent if arg(1)==101 else child if arg(1)==102 else 0
 elif op==2:ret=names+64 if arg(1)==0 else names+80
 elif op==3:
  assert bytes(cpu.mem_read(arg(1),12))==f(2,4,6);log(19);ret=2 if raw[8] else 0
  if raw[18]:cpu.mem_write(names+64,b'overridden\0')
 elif op==4:
  assert bytes(cpu.mem_read(arg(1),12))+bytes(cpu.mem_read(arg(2),12))==f(2,4.5,6,2,2.5,6)
  a=arg(3);assert readx(a+24)==0x3f800000 and readx(a+48)==0xffffffff and readx(a+56)==0
  cpu.mem_write(a,f(2,3,6)+w(*raw[20:23])+f(.5 if raw[10] else 1)+w(0x111,0x222)+f(7,8,9)+w(201 if raw[11] else -1,0x333,0x444,0x1234 if raw[13] else 0,0x555));log(17)
 elif op==5:
  log(18,arg(1));cpu.mem_write(floatret,b'\xd9\x05'+w(floatret+16)+b'\xc3');put(floatret+16,raw[14]);cpu.reg_write(UC_X86_REG_EIP,floatret);return
 elif op==6:
  assert arg(1)==source;name=bytes(cpu.mem_read(arg(2),64)).split(b'\0')[0];assert name in (b'',b'death_test');log(16,int(bool(name)))
  put(source+4,readx(source+4)|2|(0 if raw[7] else 0x400));ret=0 if raw[15] else corpse
 cpu.reg_write(UC_X86_REG_EAX,ret);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,readx(sp))
x.hook_add(UC_HOOK_CODE,hook);x.mem_write(stub,b'\xc3'*(7*16));put(backend,*[stub+i*16 for i in range(7)],0x99)
x.mem_write(names,b'corpse.v3d\0');x.mem_write(names+64,b'death_test\0');x.mem_write(names+80,b'\0')
for i,(packed,want) in enumerate(zip(inputs,expected)):
 raw=struct.unpack('<32I',packed);x.mem_write(names+64,b'death_test\0');trace.clear();x.mem_write(source,bytes(160));x.mem_write(corpse,b'\xa5'*276);put(corpse+116,200)
 put(source,100,raw[19],0x100000 if raw[1] else 0,(0x80 if raw[5] else 0)|(0x4000000 if raw[17] else 0)|0x201,101 if raw[2] else -1,raw[6],42 if raw[4] else -1,raw[16],raw[9],raw[0],names if raw[7] else names+80)
 x.mem_write(source+44,f(2,4,6));x.mem_write(source+56,w(*raw[23:32]));x.mem_write(source+92,b'\xa5'*68)
 put(parent,101,-1,0x100000 if raw[1] else 0,0x42280000);put(child,102,100,0x100000 if raw[1] else 0,0x42280000);put(player,100,0xa5a5a501)
 x.mem_write(stack,w(stop,source,backend));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX),readx(source+4),readx(source+12),readx(source+20),readx(source+28),readx(corpse+112),readx(player),readx(player+4),readx(child+12))+bytes(x.mem_read(source+56,104))+w(len(trace)//4,*trace,*([0]*(128-len(trace))))
 assert got==want,('NXDK',i,[(j,struct.unpack_from('<I',got,j)[0],struct.unpack_from('<I',want,j)[0]) for j in range(0,656,4) if got[j:j+4]!=want[j:j+4]])
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full ordinary-SP418f80 orchestration: original versus PC/NXDK fields, basis/support copies and ordered normalized resource traces. Supplied damage, attachments, explosion, geometry queries/area, construction/drop/burn backends; no live scene or native XEMU claim. Network modes excluded.')
(root/'artifacts/death-finalizer-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
