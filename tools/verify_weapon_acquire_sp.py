"""Unhooked normal-SP4030d0 inventory initializer against PC and compiled NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v])
word=lambda c,a:struct.unpack('<I',bytes(c.mem_read(a,4)))[0]
def load(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();c=Uc(UC_ARCH_X86,UC_MODE_32);c.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);c.mem_write(p.OPTIONAL_HEADER.ImageBase,im);c.mem_map(B,0x10000);return c
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=load(exe);x=load(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_weapon_acquire_sp\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
calls=0;weapon=fail=0
observed=0
def observe(c,at,size,data):
 global observed
 observed+=1
u.hook_add(UC_HOOK_CODE,observe,begin=0x401470,end=0x401470)
def original(blob):
 global calls
 ammo,capacity,magazine,unused,quantity,unusedfail=struct.unpack('<6i',blob[448:]);u.mem_write(B,bytes(0x5000));u.mem_write(B,w(B+0x4000));u.mem_write(0x64ecb9,b"\0");u.mem_write(0x6fc4d8,b"\0");u.mem_write(B+0x18c,blob[:64]);u.mem_write(B+0xc,blob[64:192]);u.mem_write(B+0x8c,blob[192:448]);u.mem_write(0x85cd2c+weapon*0x550,w(ammo));u.mem_write(0x85cf68+weapon*0x550,w(capacity));u.mem_write(0x85cd90+weapon*0x550,w(magazine));u.mem_write(STACK,w(STOP,B,weapon,quantity));u.reg_write(UC_X86_REG_ESP,STACK);calls=0;u.emu_start(0x4030d0,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return w(0)+bytes(u.mem_read(B+0x18c,64))+bytes(u.mem_read(B+0xc,128))+bytes(u.mem_read(B+0x8c,256))+w(calls)
def compiled(blob):
 global calls
 x.mem_write(B,blob[:448]);x.mem_write(B+0x1000,blob[448:460]);x.mem_write(STACK,w(STOP,B,B+0x1000,weapon,word_from(blob,464)));x.reg_write(UC_X86_REG_ESP,STACK);calls=0;x.emu_start(entry,STOP,count=10000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B,448))+w(calls)
word_from=lambda b,o:struct.unpack_from('<I',b,o)[0]
rng=random.Random(0x4030d0);cases=[];expected=[];notifications=0
for n in range(2048):
 weapon=rng.randrange(64);owned=bytearray(rng.choice([0,0,1,2,255]) for _ in range(64));magazine=rng.choice([-1,0,1,30,100]);ammo=rng.choice([-1,0,31]) if magazine<1 else rng.randrange(32);quantity=rng.choice([-2147483648,-2,-1,0,1,30,100,2147483647]);capacity=rng.choice([-1,0,1,100,2147483647]);blob=bytes(owned)+w(*[rng.choice([-2147483648,-1,0,10,2147483647]) for _ in range(96)])+w(ammo,capacity,magazine,weapon,quantity,0);fail=0;result=original(blob);actual=compiled(blob);assert actual==result,(n,blob.hex(),result.hex(),actual.hex());notifications+=struct.unpack('<I',result[-4:])[0];cases.append(blob);expected.append(result)
# Bounds failures preserve inventory; already-owned bypasses descriptor validation.
for weapon,ammo,magazine,quantity in [(-1,0,30,10),(64,0,30,10),(0,32,0,10),(0,-1,30,10),(0,32,30,10)]:
 blob=bytes(448)+w(ammo,100,magazine,weapon,quantity,0);fail=0;result=compiled(blob);assert result==w(-4)+bytes(448)+w(0);cases.append(blob);expected.append(result)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_weapon_probe.exe'),'--acquire-sp'],input=b''.join(cases));assert pc==b''.join(expected)
assert observed==840
report=dict(result='PASS',original_pc_nxdk_cases=2048,notifications_observed=observed,bounds_guards=5,callback_failures=0,scope='Full original4030d0 with actual401470/42cce0/4895d0 and40a510/40a520. Normal SP globals64ecb9=0,6fc4d8=0; observer counts notices without changing execution. No service substitution. Full inventory compared with compiled PC/NXDK wrapper, signed arithmetic and five bounds guards. Equip/reset, alternate mode and scene ownership excluded.')
(root/'artifacts/weapon-acquire-sp.json').write_text(json.dumps(report,indent=2));print(report)
