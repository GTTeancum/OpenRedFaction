"""Original40c2a0 waypoint-index transition versus PC/NXDK, including retained bytes."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
B=0x30000000;STACK=B+0x2000;STOP=B+0x3000

def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(B,0x10000);return m
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_advance\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def run(m,addr,data):
 m.mem_write(B,data);m.mem_write(STACK,w(STOP,B));m.reg_write(UC_X86_REG_ESP,STACK);m.emu_start(addr,STOP,count=1000);assert m.reg_read(UC_X86_REG_EIP)==STOP and m.reg_read(UC_X86_REG_ESP)==STACK+4
 return w(m.reg_read(UC_X86_REG_EAX))+bytes(m.mem_read(B,372))
rng=random.Random(0x40c2a0);commands=[];expected=[];advanced=0
for i in range(2048):
 data=bytearray(rng.randbytes(372));count=rng.choice([0,1,2,3,4,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32)]);current=rng.choice([0,1,2,3,4,0x7fffffff,0x80000000,0xffffffff,(count-1)&0xffffffff,rng.getrandbits(32)])
 data[:4]=w(count);data[24:28]=w(current);want=run(u,0x40c2a0,bytes(data));assert run(x,entry,bytes(data))==want,i
 advanced+=want[:4]==w(0);commands.append(bytes(data));expected.append(want)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--route-advance'],input=b''.join(commands));assert actual==b''.join(expected),'PC'
report=dict(result='PASS',original_pc_nxdk_cases=2048,advanced=advanced,scope='Full unhooked40c2a0, exact return and all372 route-prefix bytes.0..4 counts, end boundaries, signed-index extremes and count-minus-one word wrap. No arrival classification or movement dispatch.')
(root/'artifacts/navigation-advance.json').write_text(json.dumps(report,indent=2));print(report)
