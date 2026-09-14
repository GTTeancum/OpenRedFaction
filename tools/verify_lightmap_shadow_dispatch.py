"""Compare original dirty-mask scheduling with PC and compiled NXDK dispatch."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EDI
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;OWNER=B+0x7000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_lightmap_shadow_dispatch_masks\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def stop(u,a,size,ctx):u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop,begin=0x4f2c74,end=0x4f2c74)
def selected(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,index=struct.unpack('<2I',u.mem_read(sp,8));u.reg_write(UC_X86_REG_EAX,B+0x1000+index*128);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
def original_render(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,_,_,source,_,mask=struct.unpack('<6I',u.mem_read(sp,24));index=(source-B-0x1000)//128;mode=1 if a==0x4f4590 else 2;u.mem_write(mask,bytes([index+mode*16]));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
def native_render(u,a,size,ctx):
 sp=u.reg_read(UC_X86_REG_ESP);ret,_,index,mode,mask,capacity=struct.unpack('<6I',u.mem_read(sp,24));u.mem_write(mask,bytes([index+mode*16]));u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,ret)
o.hook_add(UC_HOOK_CODE,selected,begin=0x4da0a0,end=0x4da0a0)
for a in (0x4f4590,0x4f4280):o.hook_add(UC_HOOK_CODE,original_render,begin=a,end=a)
x.mem_write(STOP+32,b"\xc3") # Valid callback stub for instruction fetch before the hook.
x.hook_add(UC_HOOK_CODE,native_render,begin=STOP+32,end=STOP+32)
rng=random.Random(0x4f29b1);inputs=[];responses=[];updates=0
for i in range(2048):
 count=8;dirty=i%256;mode=(i//256)%8;width=2+i%7;height=2+(i//7)%7;stride=64;size=512;modes=[rng.randrange(4) for j in range(8)];initial=rng.randbytes(512)
 data=w(count,dirty,mode,width,height,stride,size,*modes)+initial;o.mem_write(OWNER,bytes(124));o.mem_write(OWNER+8,w(dirty));o.mem_write(OWNER+24,w(width,height));o.mem_write(OUT,initial);o.mem_write(STACK,bytes(512));o.mem_write(STACK+0x78,w(mode));o.mem_write(STACK+0x2c,w(count))
 for j,m in enumerate(modes):o.mem_write(B+0x1000+j*128+80,w(m));o.mem_write(0x5a3d3c+j*4,w(OUT+j*stride))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_EDI,count);o.emu_start(0x4f29b1,0x4f2aaa,count=1000000);end=o.reg_read(UC_X86_REG_EIP);assert end in (0x4f2aaa,0x4f2c74);changed=int(end==0x4f2aaa);expected=bytes(o.mem_read(OUT,512))
 x.mem_write(B,data);x.mem_write(OWNER,w(B+60,size,stride,width,height,B+28,count,dirty,mode));x.mem_write(OUT,w(0xffffffff));x.mem_write(STACK,w(STOP,OWNER,STOP+32,0,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(OUT,4))==w(changed) and bytes(x.mem_read(B+60,512))==expected,i
 inputs.append(data);responses.append(w(0,changed)+expected);updates+=changed
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-shadow-dispatch'],input=b''.join(inputs))==b''.join(responses)
# Invalid capacities/counts must not publish changed or write any mask.
guards=0
for descriptor in (
 w(B+60,512,64,2,2,B+28,64,2,1),
 w(B+60,511,64,2,2,B+28,8,2,1),
 w(B+60,512,64,65,2,B+28,8,2,1),
 w(B+60,512,64,2,2,0,8,2,1),
):
 x.mem_write(B+60,initial);x.mem_write(OWNER,descriptor);x.mem_write(OUT,w(0xffffffff));x.mem_write(STACK,w(STOP,OWNER,STOP+32,0,OUT));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(entry,STOP,count=1000000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(OUT,4))==w(0xffffffff) and bytes(x.mem_read(B+60,512))==initial
 guards+=1
report=dict(result='PASS',original_pc_nxdk_dispatches=len(inputs),nxdk_guards=guards,requested_accumulations=updates,original_sha256=sha,scope='Original4f29b1..4f2aaa dispatch and logical mask reset. Cached source lookup and projected/ray render bodies replaced by matching deterministic callbacks. All dirty bytes and render modes0..7, source modes0..3. Confirms scheduling, changed flag and untouched guard/padding; ray-test implementation and live dirty ownership remain external.')
(root/'artifacts/lightmap-shadow-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
