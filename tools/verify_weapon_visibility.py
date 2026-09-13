"""Original421c40 visibility/model prefix against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
B=0x30000000;S=B+0xff000;STOP=B+0xfff00;E=B+0x10000;C=B+0x20000;P=B+0x22000;L=B+0x25000;LC=B+0x26000;M=B+0x30000;O=B+0x40000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x100000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_world_visibility\s+([0-9a-fA-F]+)',mp)[1],16)
def get(c,a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={};calls=0
assert get(u,0x486cc8)==0x486c9f
u.mem_write(L+0x24,w(0));u.mem_write(L+0x294,w(LC))
def lookup(c,a,size,data):
 global calls
 calls+=1;sp=c.reg_read(UC_X86_REG_ESP);assert get(c,sp+4)==55
 c.reg_write(UC_X86_REG_EAX,L if state['kind']>=0 else 0);c.reg_write(UC_X86_REG_EIP,get(c,sp));c.reg_write(UC_X86_REG_ESP,sp+4)
def finish(c,a,size,data):c.emu_stop()
u.hook_add(UC_HOOK_CODE,lookup,begin=0x426fc0,end=0x426fc0);u.hook_add(UC_HOOK_CODE,finish,begin=0x421d47,end=0x421d47)
rng=random.Random(0x421c40);inputs=[];outputs=[];visible=0
for k in range(4096):
 flags=rng.getrandbits(32);cl=rng.getrandbits(32);inv=rng.getrandbits(32);weapon=rng.choice([-1,64,rng.randrange(64)]);attachment=rng.choice([-1,-1,7]);kind=rng.choice([-1,0,1,4,7]);present=rng.randrange(2);p44=rng.choice([0,1,2,256,257]);p3c=rng.choice([0,1,2,256,257]);special=rng.choice([weapon,3]);override=rng.choice([0,0x123456])
 if k%4==0:flags&=~0x801;cl|=8;inv&=~0x200;attachment=-1;kind=0;present=0;weapon=k%64
 values=[flags,cl,inv,weapon,attachment,kind,present,p44,p3c,special,override];raw=w(*values)
 rows=[[rng.randrange(2),rng.choice([0,0x1234,0x5678]),-1,-1] for _ in range(64)];models=b''.join(w(*r) for r in rows)
 u.mem_write(E+0x810,w(flags));u.mem_write(E+0x294,w(C));u.mem_write(C+0x724,w(cl));u.mem_write(E+0x7d0,w(inv));u.mem_write(E+0x2a4,w(weapon));u.mem_write(E+0x75c,w(attachment));u.mem_write(E+0x200,w(55));u.mem_write(LC+0x1b4,w(kind));state['kind']=kind
 u.mem_write(E+0x1430,w(P if present else 0));u.mem_write(P+0x1044,bytes([p44&255]));u.mem_write(P+0x103c,bytes([p3c&255]));u.mem_write(0x87210c,w(special));u.mem_write(0x872478,w(override))
 for i,r in enumerate(rows):u.mem_write(0x85cd08+i*0x550+0x28,w(0,B+0x8000 if r[0] else 0,r[1]))
 u.mem_write(B+0x8000,b'model.v3d\0');u.mem_write(S,w(STOP,E));u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x421c40,STOP,count=100000)
 at=u.reg_read(UC_X86_REG_EIP);assert at in (STOP,0x421d47);model=u.reg_read(UC_X86_REG_EBP) if at==0x421d47 else 0;visible+=bool(model)
 expected=w(0,get(u,E+0x810))+raw[4:]+w(model)
 x.mem_write(B,raw);x.mem_write(M,models);x.mem_write(O,w(0xdeadbeef));x.mem_write(S,w(STOP,B,M,O));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,44))+bytes(x.mem_read(O,4));assert got==expected,k
 inputs.append(raw+models);outputs.append(expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--world-visibility'],input=b''.join(inputs))==b''.join(outputs)
for args in ([0,M,O],[B,0,O],[B,M,0]):
 x.mem_write(B,raw);x.mem_write(O,w(0xdeadbeef));x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0xfffffffc
 assert bytes(x.mem_read(B,44))==raw and get(x,O)==0xdeadbeef
report=dict(result='PASS',original_cases=4096,port_guards=3,models_selected=visible,linked_lookups=calls,original_sha256=digest,scope='Original421c40 prefix through421d47 or actual hidden return, including actual predicates, linked object486c90 classification and model accessor. Only426fc0 handle lookup supplied. Exact PC/NXDK flags and selected model, player low bytes and overrides. Stable resolved linked class; hand iteration, draw state/recoil, final flag publication and live scene binding excluded.')
(root/'artifacts/weapon-visibility.json').write_text(json.dumps(report,indent=2));print(report)
