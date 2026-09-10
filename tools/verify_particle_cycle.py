"""Original emitter cycle branches, with parser return values supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EAX
def words(*v):return struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;stack=base+0xe000;stop=base+0xf000
entry=int(re.search(r'_rf_particle_cycle_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
labels=[0x59fe7c,0x59fe8c,0x59fea0,0x59feac,0x59fec0,0x59fecc]
current=0;seen=[]
for i in range(4):u.mem_write(base+0x2100+16*i,b'\xd9\x05'+words(base+0x2000+4*i)+b'\xc3')
def parser(m,address,size,data):
 global current
 if address not in (0x5126a0,0x5126f0,0x512920):return
 sp=m.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',m.mem_read(sp,4))[0]
 if address==0x5126a0:
  current=labels.index(struct.unpack('<I',m.mem_read(sp+4,4))[0]);seen.append(current);pop=8
 elif address==0x5126f0:m.reg_write(UC_X86_REG_EAX,(initially_on,alternate)[current]);pop=4
 else:
  m.mem_write(base+0x2000,words(*timing))
  m.reg_write(UC_X86_REG_EIP,base+0x2100+16*(current-2));return
 m.reg_write(UC_X86_REG_ESP,sp+pop);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,parser)
rng=random.Random(49771);commands=bytearray();expected=bytearray();count=0
for initially_on in (0,1,2,255,256,257,0xffffffff):
 for alternate in (0,1,2,255,256,257,0xffffffff):
  for emitter in (0,0x10,0x20,0xffff):
   for timing in ((0,0,0,0),(0x3f800000,0x40000000,0xbf800000,0x80000000)):
    particle=rng.getrandbits(32);secondary=rng.getrandbits(32);seen.clear()
    u.mem_write(base,bytes([0xa5])*128);u.mem_write(base+0x34,struct.pack('<H',emitter));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x49771b,0x4977c3,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==0x4977c3 and u.reg_read(UC_X86_REG_ESP)==stack
    assert seen==(list(range(6)) if alternate&255==1 else [0,1])
    original=words(struct.unpack('<H',u.mem_read(base+0x34,2))[0],particle,secondary)+bytes(u.mem_read(base+0x54,16));expected.extend(original)
    command=words(emitter,particle,secondary,initially_on,alternate,*timing);commands.extend(command);x.mem_write(base,command)
    x.mem_write(stack,words(stop,base,initially_on,alternate,base+20,base+20));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,12))+bytes(x.mem_read(base+20,16))==original,(initially_on,alternate,emitter,timing,original.hex(),(bytes(x.mem_read(base,12))+bytes(x.mem_read(base+20,16))).hex())
    count+=1
probe=root/'build/pc/Release/rf_effect_probe.exe'
assert subprocess.check_output([str(probe),'--particle-cycle'],input=commands)==expected
report=dict(result='PASS',cases=count,original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original 49771b..4977c3 with supplied label, boolean and float parser results; exact PC/NXDK flags and timing. Full parsing and emitter simulation excluded.')
(root/'artifacts/particle-cycle-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

