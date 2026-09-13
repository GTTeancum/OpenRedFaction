import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
B=0x30000000;S=B+0xe000;STOP=B+0xff00;E=B+0x1000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x10000);u.reg_write(UC_X86_REG_FPCW,0x27f);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_recoil_basis\s+([0-9a-fA-F]+)',mp)[1],16)
rng=random.Random(0x4fbf30);inputs=[];expected=[];nx=[]
for k in range(2048):
 basis=struct.pack('<9f',*[rng.uniform(-2,2) for _ in range(9)]);angle=struct.pack('<f',rng.uniform(-3.14,3.14) if k%9 else (-0.0 if k%2 else 0.0));hand=k%2
 u.mem_write(S+0x18,basis);u.mem_write(E+0x13c4,angle);u.reg_write(UC_X86_REG_ESI,E);u.reg_write(UC_X86_REG_EDI,hand);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x421d85,0x421df6,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x421df6
 expected.append(bytes(u.mem_read(S+0x18,36)));inputs.append(basis+angle+w(hand))
 x.mem_write(B,basis);x.mem_write(S,w(STOP,B,struct.unpack('<I',angle)[0],hand,B+128));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000);assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0
 nx.append(bytes(x.mem_read(B+128,36)))
raw=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--recoil-basis'],input=b''.join(inputs));pc=[]
for k in range(len(inputs)):
 assert raw[k*40:k*40+4]==w(0);pc.append(raw[k*40+4:(k+1)*40])
for label,results in [('pc',pc),('nxdk',nx)]:
 bad=[k for k,(a,b) in enumerate(zip(results,expected)) if a!=b]
 worst=max(abs(a-b)/max(1,abs(b)) for got,want in zip(results,expected) for a,b in zip(struct.unpack('<9f',got),struct.unpack('<9f',want)))
 print(label,'nonexact',len(bad),'worst scaled error',worst,'first',bad[:3]);assert worst==0
guards=[basis+angle+w(-1),basis+angle+w(2),basis+w(0x7fc00000,0),basis+w(0x7f800000,0),w(0x7fc00000)+basis[4:]+angle+w(0)]
for raw_guard in guards:
 x.mem_write(B,raw_guard[:36]);x.mem_write(B+128,b'\xa5'*36);a,h=struct.unpack('<2I',raw_guard[36:])
 x.mem_write(S,w(STOP,B,a,h,B+128));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B+128,36))==b'\xa5'*36
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--recoil-basis'],input=b''.join(guards))==(w(-4)+b'\xa5'*36)*len(guards)
x.mem_write(B,inputs[-1][:36]);a,h=struct.unpack('<2I',inputs[-1][36:]);x.mem_write(S,w(STOP,B,a,h,B));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=100000)
assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(B,36))==expected[-1]
report=dict(result='PASS',cases=2048,port_guards=6,exact=True,original_sha256=digest,scope='Unhooked421d85..421df6 recoil, axis-angle4fbf30 and ordered product40ea80 versus PC/NXDK. Both signs and signed zero, non-normalized bases, recoil within[-pi,pi], exact output bytes under x87 control0x27f. Does not establish arbitrary huge-angle behavior, live firing or draw submission.')
(root/'artifacts/weapon-recoil.json').write_text(json.dumps(report,indent=2));print(report)
