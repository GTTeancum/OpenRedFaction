"""Original special lightmap border replication vs PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_ESI
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_copy_special_border\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)

OWNER=B+0x5000;VIEW=B+0x6000
rng=random.Random(0x4f3e5d);inputs=[];responses=[]
for i in range(450):
 width=2+i%15;height=2+(i//15)%15
 # Arbitrary words prove that border copying retains NaNs and signed zeros.
 data=w(width,height,256)+w(*(rng.getrandbits(32) for j in range(768)));inputs.append(data)
 for j,a in enumerate([0x1431de0,0x14b23e0,0x13f1de0]):o.mem_write(a,data[12+j*1024:12+(j+1)*1024])
 o.mem_write(OWNER,bytes(80));o.mem_write(OWNER+24,w(width,height));o.reg_write(UC_X86_REG_ESI,OWNER);o.reg_write(UC_X86_REG_ESP,STACK)
 o.emu_start(0x4f3e5d,0x4f3f4c,count=100000);assert o.reg_read(UC_X86_REG_EIP)==0x4f3f4c
 expected=b''.join(bytes(o.mem_read(a,1024)) for a in [0x1431de0,0x14b23e0,0x13f1de0])
 x.mem_write(B,data);x.mem_write(VIEW,w(B+12,B+1036,B+2060));assert call(x,entry,[VIEW,width,height,256])==0
 assert bytes(x.mem_read(B+12,3072))==expected,i;responses.append(w(0)+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-special-border']
assert subprocess.check_output(probe,input=b''.join(inputs))==b''.join(responses)
for width,height,capacity in [(1,2,256),(2,1,256),(16,16,255),(0xffffffff,2,256)]:
 data=w(width,height,capacity)+inputs[0][12:];x.mem_write(B,data);before=bytes(x.mem_read(B+12,3072));status=call(x,entry,[VIEW,width,height,capacity]);assert status!=0 and bytes(x.mem_read(B+12,3072))==before
 assert subprocess.check_output(probe,input=data)==w(status)+before
report=dict(result='PASS',original_pc_nxdk_grids=len(inputs),guards=4,original_sha256=sha,scope='Original4f3e5d..4f3f4a unhooked border-copy instructions, all2..16 dimension pairs twice with arbitrary float bit patterns and untouched tails. Allocation cleanup and complete special-grid sampling excluded.')
(root/'artifacts/lightmap-special-border.json').write_text(json.dumps(report,indent=2));print(report)
