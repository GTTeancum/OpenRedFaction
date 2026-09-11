"""Original named timing-marker registration versus PC and NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;model=b+0x2000;nameptr=b+0x6000;stack=b+0xe000;stop=b+0xf000
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x20000);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_motion_marker_register\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x51cd30);payload=[];expected=[];changed=0
names=[b'',b'left',b'right',b'LEFT',b'footstep_left',b'footstep_right',b'123456789012345']
for i in range(1500):
 raw=bytearray(rng.randbytes(124))
 for offset in (0x40,0x54):
  name=rng.choice(names);raw[offset:offset+len(name)+1]=name+b'\0'
 name=rng.choice(names);frame=rng.choice([0.,-0.,1.,5.,19.,-.5,rng.uniform(-10000,10000)])
 f=struct.pack('<f',frame);payload.append(bytes(raw)+name.ljust(16,b'\0')+f)
 for m in (u,x):
  m.mem_write(b,bytes(raw));m.mem_write(nameptr,name+b'\0');m.reg_write(UC_X86_REG_FPCW,0x027f)
 u.mem_write(model+0xf5c,pack(b));u.mem_write(stack,pack(stop,0,nameptr)+f)
 u.reg_write(UC_X86_REG_ECX,model);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x51cd30,stop,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==stop
 want=bytes(u.mem_read(b,124));changed+=want!=raw;expected.append(pack(0)+want)
 x.mem_write(stack,pack(stop,b,nameptr)+f);x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(b,124))==want,(i,name,frame)
# Port input guards, outside the safe original domain.
guards=[]
for name,frame in [(b'x'*16,1.),(b'left',float('nan')),(b'left',float('inf')),(b'left',1e20)]:
 raw=bytes(124);guards.append(raw+name.ljust(16,b'\0')+struct.pack('<f',frame))
raw=bytearray(124);raw[0x40:0x50]=b'x'*16;guards.append(bytes(raw)+b'left'.ljust(16,b'\0')+struct.pack('<f',1.))
pc=subprocess.check_output([str(root/'build/pc/Release/rf_motion_file_probe.exe'),'--marker-register'],input=b''.join(payload+guards))
assert pc[:len(expected)*128]==b''.join(expected)
for i,wire in enumerate(guards):
 result=pc[(len(expected)+i)*128:(len(expected)+i+1)*128]
 assert struct.unpack('<i',result[:4])[0]!=0 and result[4:]==wire[:124]
 x.mem_write(b,wire[:124]);x.mem_write(nameptr,wire[124:140]);x.mem_write(stack,pack(stop,b,nameptr)+wire[140:])
 x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(b,124))==wire[:124]
report=dict(result='PASS',cases=len(payload),mutating_cases=changed,guards=len(guards),original_sha256=digest,scope='Full original51cd30 and actual51ccb0/573528 callees, no stubs, x87 precision53. Exact whole descriptor bytes versus PC/NXDK, case-sensitive duplicates and full slots preserve times. Table footstep parsing and live playback consumption excluded.')
(root/'artifacts/motion-markers.json').write_text(json.dumps(report,indent=2));print(report)
