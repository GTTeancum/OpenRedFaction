"""Full40b2e0 candidate geometry versus shared PC/compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;node=base+0x100;out=base+0x200;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<%dI'%len(v),*(x&0xffffffff for x in v))
f=lambda *v:struct.pack('<%df'%len(v),*v)
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,0x10000);return u
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_entity_navigation_single\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(cpu,address,args):
 cpu.mem_write(stack,w(stop,*args));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
rng=random.Random(0x40b2e0);commands=[];expected=[];counts=[0,0,0]
for case in range(4096):
 pos=[rng.uniform(-8,8) for _ in range(3)];center=[rng.uniform(-8,8) for _ in range(3)];radius=rng.uniform(-1,4);height=rng.uniform(-2,8);cr=rng.uniform(-1,12);ch=rng.uniform(-2,20)
 if case<1024:
  center=[0,0,0];pos=[rng.choice((0,1,2,3,4)),rng.choice((-2,-1,0,1,2)),0];radius=rng.choice((0,1,2));height=2;cr=rng.choice((0,1,2,3,4));ch=4
 mode=rng.choice((0,1,2,256,257,0xffffffff));seed=bytearray(rng.randbytes(0x44));seed[:12]=f(*center);seed[0x1c:0x24]=f(cr,ch)
 wire=f(*pos,radius,height)+w(mode)+seed;args=list(struct.unpack('<6I',wire[:24]));u.mem_write(base,wire[:12]);u.mem_write(node,bytes(seed))
 result=call(u,0x40b2e0,[base,args[3],args[4],mode,node]);assert result in (0,1,2);after=bytes(u.mem_read(node,0x44));counts[result]+=1
 for a,z in ((0,12),(24,56),(60,68)):assert after[a:z]==seed[a:z]
 x.mem_write(base,wire[:12]);x.mem_write(node,bytes(seed));x.mem_write(out,w(99))
 assert call(x,entry,[base,args[3],args[4],mode,node,out])==0
 assert bytes(x.mem_read(out,4))==w(result) and bytes(x.mem_read(node,0x44))==after,(case,result,after.hex(),bytes(x.mem_read(node,0x44)).hex())
 commands.append(wire);expected.append(w(0,result)+after)
for offset in (0,4,8,12,16,24,28,32,52,56):
 wire=bytearray(commands[0]);wire[offset:offset+4]=w(0x7fc12345);args=list(struct.unpack('<6I',wire[:24]));x.mem_write(base,bytes(wire[:12]));x.mem_write(node,bytes(wire[24:]));x.mem_write(out,w(99))
 status=call(x,entry,[base,args[3],args[4],args[5],node,out]);assert status!=0 and bytes(x.mem_read(node,0x44))==wire[24:] and bytes(x.mem_read(out,4))==w(99)
 commands.append(bytes(wire));expected.append(w(status,99)+wire[24:])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--navigation-single'],input=b''.join(commands));assert actual==b''.join(expected),'PC mismatch'
report=dict(result='PASS',cases=4096,classifications=counts,nonfinite_guards=10,scope='Full original40b2e0 and actual vector/squared-distance callees, no hooks;53-bit x87 precision. Exact classifications and full68-byte node records versus PC/compiled NXDK. Finite randomized and boundary coordinates/dimensions, negative dimensions, all low-byte adjustment routes and preserved metadata. Nonfinite inputs are port rejections. No candidate eligibility, pair geometry, list selection, alias inputs, scene or live XEMU navigation.')
(root/'artifacts/navigation-single-verification.json').write_text(json.dumps(report,indent=2));print(report)
