"""Compare original skeletal constructor registration with PC/NXDK ring publication."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
b=0x30000000;stack=b+0x70000;stop=b+0x71000

def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,im);m.mem_map(b,0x80000);m.mem_map(0,4096);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary);head=b+0x50000;active=b+0x51000
read=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
entry=int(re.search(r'\s_rf_model_skeletal_register\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
expected=[]
def run(limit,node):
 x.mem_write(stack,w(stop,node,head,limit));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop;return x.reg_read(UC_X86_REG_EAX)
for count in range(33):
 node=b+count*0x2000;u.mem_write(node,b'\xa5'*0x2000)
 for i in range(count):u.mem_write(b+i*0x2000+0x1d54,w(b+((i+1)%count)*0x2000,b+((i+count-1)%count)*0x2000))
 u.mem_write(0x181bdb8,w(b if count else 0));u.mem_write(0,w(0));u.mem_write(stack,w(stop,b+0x60000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,node)
 u.emu_start(0x51ae90,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop and read(u,node+0x1d50)==b+0x60000
 links=[]
 for i in range(33):links.extend((read(u,b+i*0x2000+off)-b)//0x2000 if i<=count else 0xffffffff for off in (0x1d54,0x1d58))
 want=w(0,(read(u,0x181bdb8)-b)//0x2000,*links);expected.append(want)
 x.mem_write(b,bytes(33*16));x.mem_write(active,b'\xa5'*208);x.mem_write(head,w(b if count else 0))
 for i in range(count):x.mem_write(b+16*i,w(0,0,b+16*((i+1)%count),b+16*((i+count-1)%count)))
 target=b+16*count;x.mem_write(target,w(1,active,0,0))
 snapshot=lambda:bytes(x.mem_read(b,33*16))+bytes(x.mem_read(head,4))+bytes(x.mem_read(active,208))
 before=snapshot();assert run(count,target)==0xfffffffc and snapshot()==before
 assert run(33,target)==0
 xlinks=[]
 for i in range(33):xlinks.extend((read(x,b+16*i+off)-b)//16 if read(x,b+16*i+off) else 0xffffffff for off in (8,12))
 assert w(0,(read(x,head)-b)//16,*xlinks)==want
 after=snapshot();assert run(33,target)==0xfffffffc and snapshot()==after and bytes(x.mem_read(active,208))==b'\xa5'*208
pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--skeletal-register'],input=w(*range(33)));assert pc==b''.join(expected)
report=dict(result='PASS',cases=33,short_limits=33,duplicate_rejections=33,original_sha256=sha,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Full original51ae90 executes without replaced callees; comparison covers registration head/neighbors only. Shared C is not the full constructor. PC/NXDK ring agreement for0..32 prior owners; pose storage unchanged, short limits and duplicate attempts preserve state. Live registry population remains open.')
(root/'artifacts/skeletal-registration-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
