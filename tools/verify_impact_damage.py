"""Original49cd80 numeric impact damage and eligibility before region suppression."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*(i&0xffffffff for i in v))
f=lambda v:struct.pack('<f',v)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
base=0x30000000;cls=base+0x2000;mode=base+0x4000;out=base+0x6000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(origin,(len(b)+4095)//4096*4096);c.mem_write(origin,b);c.mem_map(base,65536);return c
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_impact_damage\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for a in [0x49ce1c,0x49cf34]:u.hook_add(UC_HOOK_CODE,lambda c,a,s,v:c.emu_stop(),begin=a,end=a)
rng=random.Random(0x49cd80);commands=[];expected=[];requests=0
speeds=[-100,0,6.9999995,7,7.0000005,8,9,10,11,12,14,20,100]
for threshold in [7+math.sqrt(10),7+math.sqrt(5),7+2*math.sqrt(10),7+2*math.sqrt(5)]:
 bits=struct.unpack('<I',f(threshold))[0]
 speeds.extend(struct.unpack('<f',w(bits+i))[0] for i in [-1,0,1])
for case in range(2400):
 speed=speeds[case%len(speeds)] if case<1600 else rng.uniform(-20,60)
 falling=[0,1,256,257][case//len(speeds)%4];material=[0,1,3,-1][case//100%4]
 kind=[0,1,256,257][case//400%4];flags=rng.getrandbits(32)
 wire=f(speed)+w(falling,material,kind,flags);commands.append(wire)
 seed=bytearray(0x1500);seed[0x24:0x28]=w(0);seed[0x294:0x298]=w(cls);seed[0x858:0x85c]=w(mode)
 seed[0x1d0:0x1d4]=w(material);seed[0x7c:0x80]=w(flags)
 u.mem_write(base,bytes(seed));u.mem_write(cls+0x1b4,w(1 if kind&255 else 9));u.mem_write(mode+4,w(3 if falling&255 else 1))
 u.mem_write(stack,w(stop,base)+wire[:4]);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x49cd80,stop,count=10000);end=u.reg_read(UC_X86_REG_EIP);assert end in [0x49ce1c,0x49cf34]
 eligible=int(end==0x49ce1c);requests+=eligible;amount=bytes(u.mem_read(stack+8,4))
 assert bytes(u.mem_read(base,len(seed)))==seed
 want=w(0)+amount+w(eligible);expected.append(want)
 x.mem_write(out,bytes([0xa5])*8);x.mem_write(stack,w(stop,*struct.unpack('<5I',wire),out,out+4))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(out,8))==want[4:],case
for speed in [math.nan,math.inf,-math.inf,3.4028234663852886e38]:
 wire=f(speed)+w(0,1,1,0);commands.append(wire);expected.append(w(-4)+bytes([0xa5])*8)
 x.mem_write(out,bytes([0xa5])*8);x.mem_write(stack,w(stop,*struct.unpack('<5I',wire),out,out+4))
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(out,8))==bytes([0xa5])*8
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--impact-damage'],input=b''.join(commands))
assert len(actual)==12*len(expected)
for i,want in enumerate(expected):assert actual[i*12:(i+1)*12]==want,('PC',i)
report=dict(result='PASS',original_cases=2400,eligible=requests,port_rejections=4,original_sha256=digest,
 scope='Original49cd80 entry through49ce1c region query or49cf34 exit. Actual max,42a020 and429990 predicates execute unchanged. Exact PC/NXDK amount and eligibility, adjacent threshold floats and low-byte inputs. Region suppression, health, network routing, sounds and camera effects excluded; contact material at entity+1d0 supplied explicitly.')
(root/'artifacts/impact-damage-verification.json').write_text(json.dumps(report,indent=2));print(report)
