"""Original entity record creation stage, including endgame secondary allocation."""
import hashlib,json,itertools,struct,sys,re,subprocess
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ECX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
b=0x30000000;stack=b+0xc000;first=b;second=b+0x2000;definition=b+0x4000;name=b+0x6000;level_name=b+0x6100;u.mem_map(b,65536)
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
trace=[];creates=[];excluded=main_ok=secondary_ok=special_name=matching_level=0;special_class=0
boundaries={0x42cdd0,0x4ff480,0x422360,0x5001d0,0x57c130,0x4251c0}
def hook(m,address,size,data):
 if address==0x464e5a:m.emu_stop();return
 if address not in boundaries:return
 sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp);result=0;trace.append(hex(address))
 if address==0x42cdd0:
  assert read(sp+4)==class_id;result=excluded
 elif address==0x4ff480:
  obj=m.reg_read(UC_X86_REG_ECX);assert obj in (stack+0xc0,0x646140);result=name if obj==stack+0xc0 else level_name
 elif address==0x422360:
  args=struct.unpack('<7I',m.mem_read(sp+4,28));creates.append(list(args));result=(first if main_ok else 0) if len(creates)==1 else (second if secondary_ok else 0)
 elif address==0x5001d0:
  assert read(sp+4)==stack+0x34 and read(sp+8)==0x59c960;result=special_name
 elif address==0x57c130:
  assert read(sp+4)==level_name and read(sp+8)==0x59c970;result=0 if matching_level else 1
 elif address==0x4251c0:
  assert read(sp+4)==0x59c97c;result=special_class
 m.reg_write(UC_X86_REG_EAX,result&0xffffffff);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
xp=pefile.PE(str(root/'build/xbox/main.exe'));xim=xp.get_memory_mapped_image();xbase=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xbase,(len(xim)+4095)//4096*4096);x.mem_write(xbase,xim);x.mem_map(b,65536)
entry=int(re.search(r'\s_rf_entity_loader_create\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
callback=b+0xf100;stop=b+0xf000;input_at=b+0x8000;xtrace=[];commands=[];answers=[]
def xhook(m,address,size,data):
 if address!=callback:return
 sp=m.reg_read(UC_X86_REG_ESP);ret,context,request=struct.unpack('<3I',m.mem_read(sp,12));assert context==0
 args=struct.unpack('<7I',m.mem_read(request,28));assert args[2]==0xffffffff and args[3:5]==(stack+0xd8,stack+0xf4) and args[6]==0xffffffff
 text=bytes(m.mem_read(args[1],32)).split(b'\0')[0];assert text in (b'main',b'masako_endgame')
 xtrace.append([args[0],1 if text==b'main' else 2,args[5]])
 result=(first if main_ok else 0) if len(xtrace)==1 else (second if secondary_ok else 0)
 m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,xhook)
cases=0;hist=[0,0,0];writes=0
for class_id,multi,excluded,hidden,other_flag,main_ok,special_name,matching_level,special_class,secondary_ok in itertools.product((-1,0),(0,1,2),(0,1,256),(0,1,256),(0,2,256),(0,1),(0,1),(0,1),(-1,0),(0,1)):
 u.mem_write(stack,bytes(1024));u.mem_write(stack+0x24,w(other_flag));u.mem_write(0x64ecb9,bytes((multi,)));u.mem_write(first,b'\xa5'*0x1500);u.mem_write(second,b'\xa5'*0x1500);u.mem_write(definition,b'\xcc'*0x800);u.mem_write(second+0x294,w(definition));u.mem_write(second+0x2c,w(12345))
 transform=w(*range(12));u.mem_write(stack+0xd8,transform[:12]);u.mem_write(stack+0xf4,transform[12:])
 before=[bytearray(u.mem_read(first,0x1500)),bytearray(u.mem_read(second,0x1500)),bytearray(u.mem_read(definition,0x800))]
 trace=[];creates=[];u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,class_id&0xffffffff);u.reg_write(UC_X86_REG_EBX,hidden)
 u.emu_start(0x464625,0x46474a,count=10000);assert u.reg_read(UC_X86_REG_EIP) in (0x46474a,0x464e5a)
 expected_calls=[];expected_creates=[]
 if class_id>=0:
  if multi:expected_calls.append('0x42cdd0')
  if not multi or not excluded&255:
   expected_calls+=['0x4ff480','0x422360'];flags=(2 if hidden&255 else 0)|(4 if other_flag&255 else 0)
   expected_creates.append([class_id,name,0xffffffff,stack+0xd8,stack+0xf4,flags,0xffffffff])
   if main_ok:
    struct.pack_into('<I',before[0],0x146c,0xffffffff);expected_calls.append('0x5001d0')
    if special_name&255:
     expected_calls+=['0x4ff480','0x57c130']
     if matching_level:
      expected_calls.append('0x4251c0')
      if special_class>=0:
       expected_calls.append('0x422360');expected_creates.append([special_class,0x59c984,0xffffffff,stack+0xd8,stack+0xf4,2,0xffffffff])
       if secondary_ok:
        before[1][0x7c8]=1;struct.pack_into('<I',before[1],0x814,0xa5a5a5a5|0x10);struct.pack_into('<I',before[1],0x7cc,0x41200000);struct.pack_into('<I',before[0],0x146c,12345);struct.pack_into('<I',before[2],0x728,0xcccccccc|0x100);writes+=1
 assert trace==expected_calls and creates==expected_creates,(cases,trace,expected_calls,creates,expected_creates)
 assert bytes(u.mem_read(first,0x1500))==before[0] and bytes(u.mem_read(second,0x1500))==before[1] and bytes(u.mem_read(definition,0x800))==before[2],cases
 assert bytes(u.mem_read(stack+0xd8,12))+bytes(u.mem_read(stack+0xf4,36))==transform
 expected_trace=[[a[0],1 if a[1]==name else 2,a[5]] for a in creates]
 expected_words=[int(bool(creates) and bool(main_ok)),len(creates)]+[v for row in expected_trace for v in row]+[0]*(6-3*len(creates))
 expected_words += [struct.unpack_from('<I',before[0],0x146c)[0],before[1][0x7c8],struct.unpack_from('<I',before[1],0x814)[0],struct.unpack_from('<I',before[1],0x7cc)[0],struct.unpack_from('<I',before[2],0x728)[0]]
 payload=w(class_id,hidden,other_flag,multi,excluded,special_name,matching_level,special_class,name,stack+0xd8,stack+0xf4)
 x.mem_write(input_at,payload);x.mem_write(name,b'main\0');x.mem_write(first,b'\xa5'*24);x.mem_write(second,b'\xa5'*24);x.mem_write(second,w(12345));x.mem_write(first+20,w(definition+0x728));x.mem_write(second+20,w(definition+0x728));x.mem_write(definition+0x728,w(0xcccccccc))
 x.mem_write(stack+0xd8,transform[:12]);x.mem_write(stack+0xf4,transform[12:]);x.mem_write(stack,w(stop,input_at,callback,0));x.reg_write(UC_X86_REG_ESP,stack);xtrace=[]
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual_words=[int(x.reg_read(UC_X86_REG_EAX)!=0),len(xtrace)]+[v for row in xtrace for v in row]+[0]*(6-3*len(xtrace))
 xr=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
 actual_words += [xr(first+4),x.mem_read(second+12,1)[0],xr(second+8),xr(second+16),xr(definition+0x728)]
 assert actual_words==expected_words,(cases,actual_words,expected_words)
 assert bytes(x.mem_read(input_at,44))==payload and bytes(x.mem_read(stack+0xd8,12))+bytes(x.mem_read(stack+0xf4,36))==transform
 commands.append(w(class_id,multi,excluded,hidden,other_flag,main_ok,special_name,matching_level,special_class,secondary_ok));answers.append(w(*expected_words))
 hist[len(creates)]+=1;cases+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--loader-create'],input=b''.join(commands));assert actual==b''.join(answers)
report=dict(result='PASS',cases=cases,creation_count_histogram=hist,secondary_publications=writes,original_sha256=sha,scope='Original464625..46474a (or record-skip464e5a) with resolved class ID and parsed locals. Predicate/name/class lookup and factory boundaries supplied; exact call order and seven factory arguments, shared transform pointers/input bytes, full two-actor and class storage changes. Covers main failure and immediate endgame second allocation/failure. Does not execute parsing, constructors or subsequent per-record setup, plus exact shared C PC/NXDK constructor requests and publication with resolved predicates. Excludes live scene ownership and native XEMU activation.')
(root/'artifacts/entity-loader-creation-order.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
