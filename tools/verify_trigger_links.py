"""Original SP4c0320 vs shared typed registry dispatch, effect boundaries recorded."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;stack=b+0xe000;stop=b+0xf000;callback=b+0xf100
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');original_trace=[];native_trace=[]
def original_effect(m,a,size,context):
 sp=m.reg_read(UC_X86_REG_ESP);ret,target,source,actor=struct.unpack('<4I',m.mem_read(sp,16))
 original_trace.append([8 if a==0x46aba0 else 6,target,source,actor]);m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
for addr in (0x46aba0,0x4b6760):u.hook_add(UC_HOOK_CODE,original_effect,begin=addr,end=addr)
def native_effect(m,a,size,context):
 sp=m.reg_read(UC_X86_REG_ESP);ret,ctx,kind,handle,source,actor=struct.unpack('<6I',m.mem_read(sp,24))
 native_trace.append([kind,handle,source,actor]);m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,native_effect,begin=callback,end=callback)
entry=int(re.search(r'_rf_trigger_links_dispatch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
handles=[((i+1)<<16)|i for i in range(4)];source=0x23450020;actor=0x34560021
u.mem_write(0x64ecb9,bytes(2));u.mem_write(b+0x2c,w(source));u.mem_write(b+0x2d4,w(4,4,b+0x1000));u.mem_write(b+0x1000,w(*handles))
x.mem_write(b+0x4000,w(4,b+0x4100));x.mem_write(b+0x4100,w(*handles));commands=bytearray();expected=bytearray();cases=0
for kinds in itertools.product((0,5,6,8),repeat=4):
 for suppress in (0,1,256,257):
  for i,kind in enumerate(kinds):
   obj=b+0x2000+i*0x400;u.mem_write(0x7394cc+i*4,w(obj));u.mem_write(obj+0x24,w(kind));u.mem_write(obj+0x2c,w(handles[i]))
   x.mem_write(b+i*8,w(b+0x5000+i*4,handles[i]));x.mem_write(b+0x5000+i*4,w(kind))
  original_trace.clear();u.mem_write(stack,w(stop,b,actor,suppress));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c0320,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  native_trace.clear();x.mem_write(stack,w(stop,b,b+0x4000,source,actor,suppress,callback,0));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
  assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and native_trace==original_trace,(kinds,suppress,native_trace,original_trace)
  commands.extend(w(*kinds,suppress));expected.extend(w(0,len(original_trace))+b''.join(w(*r) for r in original_trace)+bytes((4-len(original_trace))*16));cases+=1
actual=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--trigger-links'],input=commands);assert actual==expected
report=dict(result='PASS',cases=cases,scope='Original4c0320 SP routing with real handle/array callees; only downstream controller/event effects recorded. PC/NXDK exact ordered kind/handle/source/actor calls including suppression low-byte behavior. Synthetic registry, no action execution or live scene dispatch.')
(root/'artifacts/trigger-links-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
