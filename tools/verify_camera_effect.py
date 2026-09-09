"""Original camera effect timer, pre-decay cone and per-player reset."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;context=base+0x2000;camera=base+0x4000;stack=base+0xe000;stop=base+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
def machine(path):
 p=pefile.PE(str(path));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);u.mem_map(base,0x10000);return u
u=machine(exe);calls=[];cone=f(17)
def hook(m,address,size,data):
 global cone
 if address not in (0x4a5b70,0x4fae00,0x4fc960):return
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0];pop=0
 if address==0x4a5b70:m.reg_write(UC_X86_REG_EAX,base)
 elif address==0x4fae00:cone=bytes(m.mem_read(sp+16,4));calls.append('cone');pop=16
 else:calls.append('rebuild')
 m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x40db70);P=1072800000;inputs=[];expected=[];active_count=0
for i in range(600):
 now=rng.choice([0,1,P-1,P,rng.randrange(P)]);deadline=rng.choice([-1,now,(now+999)%P,(now+1000)%P,(now+1001)%P,(now-1)%P]);reset=int(i%7==0)
 raw=f(rng.uniform(-3,3),rng.uniform(0,10))+struct.pack('<i',deadline);inputs.append(raw+struct.pack('<iI',now,reset));calls.clear();cone=f(17)
 u.mem_write(base+0x8b4,raw);u.mem_write(0x5a3ed8,struct.pack('<i',now));u.mem_write(context,w(camera,base+0x3000));u.mem_write(stack,w(stop,context));u.reg_write(UC_X86_REG_ESP,stack)
 if reset:
  u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,0);u.emu_start(0x41d9af,0x41d9c7,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x41d9c7
  active=99
 else:
  u.emu_start(0x40db70,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop;assert calls in ([],['cone','rebuild']);active=int(bool(calls));active_count+=active
 expected.append(w(0)+bytes(u.mem_read(base+0x8b4,12))+cone+w(active))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_eye_probe.exe'),'--effect'],input=b''.join(inputs))
for i,e in enumerate(expected):assert actual[i*24:(i+1)*24]==e,('PC mismatch',i,actual[i*24:(i+1)*24].hex(),e.hex())
x=machine(ROOT/'build/xbox/main.exe');mapping=(ROOT/'build/xbox/main.map').read_text()
entries=[int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ['rf_camera_effect_step','rf_camera_effect_reset']]
for i,payload in enumerate(inputs):
 now,reset=struct.unpack('<iI',payload[12:]);x.mem_write(base,payload[:12]);x.mem_write(base+0x100,f(17)+w(99))
 args=w(stop,base,now,base+0x100,base+0x104) if not reset else w(stop,base,now)
 x.mem_write(stack,args);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entries[reset],stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,12))+bytes(x.mem_read(base+0x100,8));assert got==expected[i],('NXDK mismatch',i)
report=dict(result='PASS',pc_cases=len(inputs),nxdk_cases=len(inputs),active_cases=active_count,scope='40db70 with original timers/clamp, resolved-player and direction/rebuild boundaries intercepted. 41d9af..41d9c7 reset slice. Does not validate random direction or orientation rebuilding.')
(ROOT/'artifacts/camera-effect-verification.json').write_text(json.dumps(report,indent=2));print(report)
