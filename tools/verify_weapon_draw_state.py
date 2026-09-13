"""Unhooked original held-weapon draw-state setup versus compiled ports."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
B=0x30000000;S=B+0xe000;STOP=B+0xff00;E=B+0x1000;V=B+0x3000
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);a=p.OPTIONAL_HEADER.ImageBase;u.mem_map(a,(len(im)+4095)//4096*4096);u.mem_write(a,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_weapon_draw_state_prepare\s+([0-9a-fA-F]+)',mp)[1],16)
u.mem_write(0x7c763c,w(V));rng=random.Random(0x421df6);inputs=[];outputs=[]
for k in range(2048):
 raw=w(*[rng.getrandbits(32) for _ in range(20)]);basis=w(*[rng.getrandbits(32) for _ in range(9)]);special=rng.choice([0,1,2,255,256,257,513]);tint=rng.getrandbits(32)
 u.mem_write(S+0x48,raw);u.mem_write(S+0x18,basis);u.mem_write(V+0xfb0,bytes([special&255]));u.mem_write(E+0x1474,w(tint));u.reg_write(UC_X86_REG_ESI,E);u.reg_write(UC_X86_REG_ESP,S)
 u.emu_start(0x421df6,0x421e4f,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x421e4f;expected=bytes(u.mem_read(S+0x48,80))
 x.mem_write(B,raw);x.mem_write(B+0x100,basis);x.mem_write(S,w(STOP,B,special,tint,B+0x100));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==STOP and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(B,80))==expected,k
 inputs.append(raw+w(special,tint)+basis);outputs.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--draw-state'],input=b''.join(inputs))==b''.join(outputs)
for state,basis_ptr in [(0,B+0x100),(B,0)]:
 x.mem_write(B,raw);x.mem_write(S,w(STOP,state,1,tint,basis_ptr));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)==0xfffffffc and bytes(x.mem_read(B,80))==raw
# Explicitly supported alias: source matrix overlaps fields overwritten by setup.
x.mem_write(B,raw);x.mem_write(S,w(STOP,B,0,tint,B));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(B+44,36))==raw[:36]
report=dict(result='PASS',original_cases=2048,port_guards=3,original_sha256=digest,scope='Unhooked421df6..421e4f with actual500f80/500fb0/50cc00/50cc40/4fce70/40a3b0; exact80-byte PC/NXDK state, preserved constructor fields, special-view low byte, tint and arbitrary matrix bit patterns. Recoil is already supplied in the pose; no draw submission or scene binding.')
(root/'artifacts/weapon-draw-state.json').write_text(json.dumps(report,indent=2));print(report)
