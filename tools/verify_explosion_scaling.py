"""Original central explosion size gate and six-field scaling vs PC/NXDK."""
import hashlib,json,re,struct,random,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_EBP
base=0x30000000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(b)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,b);u.mem_map(base,65536);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_explosion_central_prepare\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def halt(m,a,s,d):
 if a in (0x48e87a,0x48e97c):m.emu_stop()
u.hook_add(UC_HOOK_CODE,halt)
rng=random.Random(48);commands=bytearray();expected_all=bytearray();accepted=0
for case in range(600):
 slot=case%6;size=[-1,0,0.5,1,2,float('nan')][case%6] if case<36 else rng.randint(-32,128)/16
 minimum=[0,0.5,1,-1,float('nan')][case%5];extent=rng.randint(-8,8)/16
 fields=[rng.randint(-64,128)/16 for _ in range(6)]
 definition=bytearray(2380);struct.pack_into('<I',definition,36,6);struct.pack_into('<f',definition,112+slot*76,minimum);struct.pack_into('<f',definition,700,extent);struct.pack_into('<I',definition,2368,63)
 particle=bytearray([0xa5]*184)
 for offset,value in zip((28,32,48,52,56,60),fields):struct.pack_into('<f',particle,offset,value)
 definition[712+slot*184:712+(slot+1)*184]=particle
 original_recipe=base+0x2000;original_particle=base+0x3000
 u.mem_write(original_recipe,bytes(152));u.mem_write(original_recipe+0x2c+slot*4,struct.pack('<I',slot));u.mem_write(original_recipe+0x4c+slot*4,struct.pack('<f',minimum));u.mem_write(original_recipe+0x90,struct.pack('<f',extent));u.mem_write(0x7b2770+slot*4,struct.pack('<I',original_particle));u.mem_write(original_particle,bytes(128))
 for offset,value in zip((0x20,0x24,0x38,0x3c,0x40,0x44),fields):u.mem_write(original_particle+offset,struct.pack('<f',value))
 u.mem_write(stack+0x100,struct.pack('<f',size));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,original_recipe+0x2c+slot*4);u.reg_write(UC_X86_REG_EBP,original_recipe);u.emu_start(0x48e7c5,stop,count=10000)
 success=u.reg_read(UC_X86_REG_EIP)==0x48e87a;assert success or u.reg_read(UC_X86_REG_EIP)==0x48e97c
 if success:
  accepted+=1
  for dst,src in zip((28,32,48,52,56,60),(0x20,0x24,0x38,0x3c,0x40,0x44)):particle[dst:dst+4]=u.mem_read(original_particle+src,4)
  expected=bytes(4)+particle+bytes(u.mem_read(stack+0x20,4))
 else:expected=struct.pack('<i',-3)+bytes([0xa5])*188
 expected_all.extend(expected);commands.extend(definition+struct.pack('<If',slot,size))
 x.mem_write(base,bytes(definition));x.mem_write(base+0x4000,bytes([0xa5])*188);x.mem_write(stack,struct.pack('<III f II',stop,base,slot,size,base+0x4000,base+0x4000+184));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x4000,188));assert got==expected,case
 assert bytes(x.mem_read(base,2380))==definition
probe=root/'build/pc/Release/rf_effect_probe.exe';assert subprocess.check_output([str(probe),'--explosion-scale'],input=commands)==expected_all
report=dict(result='PASS',cases=600,accepted=accepted,original_sha256=sha,scope='Unmodified original 48e7c5..48e879 or below-minimum branch, seeded definitions and size. Six scaled fields and random extent match PC/NXDK exactly, original inputs retained in shared implementation. Position/direction sampling, emitter creation and timing excluded.')
(root/'artifacts/explosion-scaling-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
