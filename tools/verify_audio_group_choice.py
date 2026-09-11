"""Original48a930 sample selection, actual CRT RNG and view routing vs PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
base=0x30000000;stack=base+0xe000;stop=base+0xf000;thread=base+0x6000
pack=lambda *x:struct.pack('<'+'I'*len(x),*x)
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase
 u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(base,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);nx=machine(root/'build/xbox/main.exe');calls=[];draws=0

def hook(uc,address,size,context):
 global draws
 if address not in (0x577eef,0x505560,0x5056a0):return
 sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 if address==0x577eef:draws+=1;uc.reg_write(UC_X86_REG_EAX,thread)
 elif address==0x505560:
  sample,flags,a,b=struct.unpack('<4I',uc.mem_read(sp+4,16));assert flags==0
  calls.append(pack(sample,0,0,0,0,a,b,flags))
 else:
  sample,pos,gain,vector,flags=struct.unpack('<5I',uc.mem_read(sp+4,20));assert vector==0x173c378 and gain==0x3f800000 and flags==0
  calls.append(pack(sample,1)+bytes(uc.mem_read(pos,12))+pack(gain,0,flags))
 uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x48a930);cases=[];expected=[];flat=0;spatial=0;totaldraws=0
for k in range(1024):
 kind=(0,0,1,255)[k%4];present=(k//4)%2;mode=(0,0,1,-1)[(k//8)%4];count=(-2147483648,-1,0,1,2,3,4,5,6,7,8)[k%11]
 seed=rng.getrandbits(32);values=[rng.getrandbits(32) for _ in range(5)];samples=[rng.getrandbits(32) for _ in range(8)]
 raw=pack(kind,present,mode&0xffffffff,*values,count&0xffffffff,seed,*samples);assert len(raw)==72;cases.append(raw)
 u.mem_write(base,bytes(0x1500));u.mem_write(base+0x24,pack(kind));u.mem_write(base+0x1430,pack(base+0x2000 if present else 0));u.mem_write(base+0x20c4,pack(base+0x3000));u.mem_write(base+0x3008,pack(mode&0xffffffff))
 u.mem_write(base+0x4000,pack(*samples));u.mem_write(thread+0x14,pack(seed))
 u.mem_write(stack,pack(stop,base,*values[:3],base+0x4000,count&0xffffffff,*values[3:]));u.reg_write(UC_X86_REG_ESP,stack)
 calls=[];draws=0;u.emu_start(0x48a930,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==stack+4
 assert len(calls)==1 and draws==int(count>1);assert pack(u.reg_read(UC_X86_REG_EAX))==calls[0][:4]
 route=struct.unpack_from('<I',calls[0],4)[0];spatial+=route;flat+=1-route;totaldraws+=draws
 expected.append(pack(0)+bytes(u.mem_read(thread+0x14,4))+calls[0])
# Invalid span: reject before advancing RNG or writing result.
raw=bytearray(cases[0]);struct.pack_into('<i',raw,32,9);cases.append(bytes(raw));expected.append(struct.pack('<i',-4)+bytes(raw[36:40])+b'\xa5'*32)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--group-choose'],input=b''.join(cases))
for k,want in enumerate(expected):assert pc[k*40:k*40+40]==want,('PC',k,pc[k*40:k*40+40].hex(),want.hex())
assert len(pc)==len(expected)*40
entry=int(re.search(r'\s_rf_audio_group_choose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for k,raw in enumerate(cases):
 nx.mem_write(base,raw);nx.mem_write(base+0x100,b'\xa5'*32);count=struct.unpack_from('<I',raw,32)[0]
 nx.mem_write(stack,pack(stop,base,base+40,8,count,base+36,base+0x100));nx.reg_write(UC_X86_REG_ESP,stack);nx.emu_start(entry,stop,count=1000)
 assert nx.reg_read(UC_X86_REG_EIP)==stop
 actual=pack(nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base+36,4))+bytes(nx.mem_read(base+0x100,32));assert actual==expected[k],('NXDK',k,actual.hex(),expected[k].hex())
report=dict(result='PASS',cases=len(cases),flat=flat,spatial=spatial,random_draws=totaldraws,original_sha256=sha,scope='Full48a930 with actual48acf0/40d740 and57312d CRT RNG; thread storage supplied,505560/5056a0 requests observed. PC/NXDK sample IDs, parameter bits, routing and RNG match. Includes signed count<=1 and one capacity rejection; shared173c378 vector/backend audio and live footstep timing remain outside adapter.')
(root/'artifacts/audio-group-choice.json').write_text(json.dumps(report,indent=2));print(report)
