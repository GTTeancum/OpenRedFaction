"""Concrete shared clutter base ownership, registry order and resource failures."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
r=lambda data,off=0:struct.unpack_from('<I',data,off)[0]
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,im)
B=0x30000000;x.mem_map(B,0x100000);R=B+0x10000;L=B+0x20000;PR=L+0x1000;O=L+0x2000;UID=O+0x100;D=O+0x1000;M=D+0x1000;BE=M+0x1000;ROWS=B+0x30000;NAME=ROWS+0x1000;CB=B+0x40000;S=B+0xe0000;STOP=S+0x1000
mapping=(ROOT/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,reginit,listinit,append,calloc,malloc,free=map(sym,('rf_clutter_base_open','rf_clutter_base_close','rf_object_registry_init','rf_object_list_init','rf_object_list_append','calloc','malloc','free'))
read=lambda a:r(x.mem_read(a,4));put=lambda a,v:x.mem_write(a,w(v))
x.mem_write(CB,b'\xc3'*96);x.mem_write(BE,w(CB,CB+16,CB+32,CB+48,0,CB+64,CB+80));x.mem_write(NAME,b'clutter.v3m\0')
live={};allocations=0;calls=0;releases=0;command=b'';fail_alloc=0

def hook(cpu,a,size,context):
 global allocations,calls,releases
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:read(sp+4+4*i);result=0
 if a in (calloc,malloc):
  n=arg(0)*(arg(1) if a==calloc else 1);allocations+=1;assert n<=532
  if allocations!=fail_alloc:
   result=B+0x60000+(allocations-1)*0x10000;assert result not in live;live[result]=n
   cpu.mem_write(result,(b'\0' if a==calloc else b'\xa5')*n)
 elif a==free:
  if arg(0):n=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*n)
 else:
  stage=(a-CB)//16+1
  if stage==6:
   assert arg(1)==r(command,4) and read(L+8)==1;releases+=1
  else:
   calls|=1<<stage;owner=read(R+7*8)
   assert owner and read(L+8)==2 and read(owner+12)==(0 if stage==1 else r(command,4)),(stage,hex(owner))
   if r(command,44)==stage:result=0xffffffff
   elif stage==1:
    assert arg(1)==r(command);cpu.mem_write(arg(5),command[4:8])
   elif stage==2:cpu.mem_write(arg(2),command[100:112]);cpu.mem_write(arg(3),command[112:116])
   elif stage==3:assert arg(2)==0 and arg(3)==0x3f800000
   elif stage==4:put(arg(2),17)
   else:cpu.mem_write(arg(2),w(ROWS,r(command,8),1,0,0))
 cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,arg(-1))
for a in (calloc,malloc,free,*range(CB,CB+96,16)):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(a,STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def snapshot():
 head=read(R+12288);count=read(R+12292)
 return [read(L+8),read(L+12),read(R+12296),count,read(R+8192+head*4),read(R+8192+((head+count-1)%1024)*4) if count else 0xffffffff]
def run(data):
 global command,allocations,calls,releases
 assert not live;command=data;allocations=calls=releases=0
 call(reginit,R);put(R+12292,r(data,36));put(R+12296,r(data,32));put(R+8192,7);put(R+8196,9)
 call(listinit,L);x.mem_write(PR,bytes(8));call(append,L,PR);put(O,0);put(UID,-1)
 # descriptor: model,kind,material,flags,allocation_flags,identifier,pos,matrix,radius
 x.mem_write(D,w(NAME if r(data) else 0,r(data),2,r(data,16),r(data,12),123)+data[48:100]);x.mem_write(M,data[116:128]);x.mem_write(ROWS,data[128:] or bytes(48))
 status=call(entry,D,R,L,UID,r(data,20),r(data,24),r(data,28),M,BE,r(data,40),O)
 owner=read(O);out=w(status,int(bool(owner)),read(UID),*snapshot());raw=None
 if owner:
  raw=bytes(x.mem_read(owner,532));used=r(raw,444);expected_bytes=532+used*24
  assert r(raw,524)==expected_bytes and r(raw,528)==expected_bytes+r(data,8)*24
  assert r(raw,8)==(r(data,32)<<16)|7 and read(R+56)==owner
  normalized=bytearray(raw);normalized[0:4]=w(1);normalized[108:116]=w(1,1);normalized[440:444]=w(int(bool(r(raw,440))))
  out+=normalized+(bytes(x.mem_read(r(raw,440),used*24)) if used else b'')
 closed=call(close,O,R,L,BE);out+=w(closed,*snapshot(),releases,calls)
 assert closed==0 and read(O)==0 and not live
 published=allocations>0 and fail_alloc!=1
 generation=(1 if r(data,32)==0x752e else r(data,32)+1) if published else r(data,32)
 assert read(UID)==(0xfffffffe if published else 0xffffffff)
 assert snapshot()==[1,2 if published else 1,generation,r(data,36),9 if published else 7,7 if published else 9 if r(data,36) else 0xffffffff]

 assert read(L)==PR and read(L+4)==PR and read(PR)==L and read(PR+4)==L
 assert call(close,O,R,L,BE)==0
 return out,raw
rng=random.Random(0x486da0);commands=[];expected=[];rows_by_case=[];success=0
for case in range(256):
 kind=(0,1,3)[case%3];count=(0,1,2,4)[(case//3)%4] if kind else 0
 data=w(kind,0 if case%11==0 else 0x56780000,count,(0,0x4000,0x10000,0x100000)[(case//12)%4],(0,0x20)[(case//48)%2],case%3,case%8,1+case%5,0x752e if case%7==0 else case+1,2,4096,0)
 data+=f(1,2,3,1,0,0,0,1,0,0,0,1,(-1,0,.5,2)[case%4],0,0,0,1.25,.25,.5,2)
 assert len(data)==128
 rows=b''.join(bytes(28)+w(-1)+f(j*.5,0,0,.5,-1)[:16] for j in range(count));assert len(rows)==count*48
 data+=rows;out,raw=run(data);assert r(out)==0
 success+=raw is not None;commands.append(data);expected.append(out);rows_by_case.append(raw)
# Service failures, short budget, empty registry and exact peak are distinct stages.
base=bytearray(commands[5]);base[0:8]=w(3,0x56780000);base[16:20]=w(0x20)
for fail in range(1,6):
 data=bytearray(base);data[44:48]=w(fail);out,_=run(bytes(data));assert r(out)==0xffffffff;commands.append(bytes(data));expected.append(out)
for free_count,budget in ((0,4096),(2,531),(2,532)):
 data=bytearray(base);data[36:44]=w(free_count,budget);out,_=run(bytes(data));assert r(out)==(0 if free_count==0 else 0xfffffffc);commands.append(bytes(data));expected.append(out)
_,raw=run(bytes(base));peak=r(raw,528)
for budget in (peak,peak-1):
 data=bytearray(base);data[40:44]=w(budget);out,_=run(bytes(data));assert r(out)==(0 if budget==peak else 0xfffffffc);commands.append(bytes(data));expected.append(out)
assert subprocess.check_output([str(ROOT/'build/pc/Release/rf_entity_assets_probe.exe'),'--clutter-base'],input=b''.join(commands))==b''.join(expected)
for fail_alloc in (1,2,3):
 out,_=run(bytes(base));assert r(out) in (0xffffffff,0xfffffffc)
fail_alloc=0
# Execute the complete original allocator for the no-authored-sphere cases.
# Other shared composition stages have their own original-backed verifiers.
g={"__file__":str(ROOT/'tools/verify_clutter_base_original.py')}
original_source=(ROOT/'tools/verify_clutter_base_original.py').read_text()
exec(original_source[:original_source.index('examples=[]')],g)
u=g['u'];put_original=g['put'];ob=g['b'];actor=g['actor'];params=g['params'];source=g['source'];verified_original=0
for index,(data,raw) in enumerate(zip(commands[:256],rows_by_case)):
 if r(data,8) or raw is None:continue
 node=0x73a880+7*12;previous=ob+0x3000
 put_original(0x7394c0,node);put_original(0x7394c4,node);u.mem_write(node,w(0x7394c0,0x7394c0,7))
 put_original(0x73d890,previous);put_original(0x73d894,previous);put_original(previous+0x10,0x73d880);put_original(previous+0x14,0x73d880)
 put_original(0x73a850,1);put_original(0x73db0c,1);put_original(0x708744,r(data,32));put_original(0x6460e8,0);put_original(0x59f7e4,0xffffffff)
 g['kind']=r(data);g['has_parent']=True;g['model_radius']=struct.unpack_from('<f',data,112)[0]
 u.mem_write(g['parent']+0x28,bytes([r(data,24)]));put_original(g['parent']+0x1f8,r(data,28))
 descriptor=bytearray(152);descriptor[:8]=w(source+0x800 if r(data) else 0,r(data));descriptor[16:20]=w(2)
 descriptor[60:108]=data[48:96];descriptor[132:136]=data[96:100];descriptor[148:152]=data[16:20]
 u.mem_write(params,bytes(descriptor));u.mem_write(source+0x800,b'clutter.v3m\0');u.mem_write(0x649f50+2*28,data[116:128]);g['allocations'].clear();g['freed'].clear();g['trace'].clear()
 assert g['call'](0x486da0,(4,0xffffffff,123,params,r(data,12),r(data,20)))==actor
 read_original=g['read'];original_body=bytes(u.mem_read(actor+0x88,0x170))
 body=original_body[:12]+original_body[0x10:0xfc]+original_body[0x108:0x128]+original_body[0x138:0x148]+original_body[0x15c:0x160]+original_body[0x164:0x16c]
 assert raw[132:440]==body,('original body',index,[(i,raw[132+i:136+i].hex(),body[i:i+4].hex()) for i in range(0,308,4) if raw[132+i:136+i]!=body[i:i+4]])
 assert r(raw,4)==read_original(actor) and r(raw,8)==read_original(actor+0x2c) and bool(r(raw,12))==bool(read_original(actor+0x80))
 assert r(raw,16)==read_original(actor+0x7c) and r(raw,124)==read_original(actor+0x78)
 assert raw[24:44]==bytes(u.mem_read(actor+0x3c,12))+bytes(u.mem_read(actor+0x34,8))
 used=read_original(actor+0x184);assert r(raw,444)==used
 owned=bytes(u.mem_read(read_original(actor+0x18c),used*24)) if used else b''
 if used:owned=owned[:20]+w(0)
 assert expected[index][568:568+used*24]==owned
 verified_original+=1
report=dict(result='PASS',cases=len(commands),original_allocator_cases=verified_original,successful=success,allocation_failures=3,owner_bytes=532,scope='Concrete PC/NXDK heap ownership, model attachment, static creation spheres and physics composition. Callback checks observe registered model publication; missing models/errors/budgets clean owners and recycle slots, including generation wrap. All owned bytes, callback stages and registry/list state match. Full original486da0 comparison for no-authored-sphere successful cases, including body and fallback bytes; supplied model resources/materials/room/parent; no live scene or native XEMU claim.')
(ROOT/'artifacts/clutter-base.json').write_text(json.dumps(report,indent=2));print(report)

