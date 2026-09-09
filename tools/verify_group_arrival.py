"""Translation arrival state and sound requests vs unchanged original block."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
source=root/'Installed_Game/RF.exe';assert hashlib.sha256(source.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();b=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(b,(len(im)+4095)//4096*4096);u.mem_write(b,im);u.mem_map(0x30000000,65536);return u
u=machine(source);base=0x30000000;stack=base+50000;stop=base+64000;vector=base+4096
offsets=[0x318,0x2e8,0x2f8,0x2fc,0x300,0x30c];sounds=[]
def observe(uc,address,size,data):sounds.append(data)
u.hook_add(UC_HOOK_CODE,observe,user_data=1,begin=0x46a120,end=0x46a120)
u.hook_add(UC_HOOK_CODE,observe,user_data=2,begin=0x46a0d0,end=0x46a0d0)
rng=random.Random(0x469da4);cases=[];expected=[];sound_counts=[0,0,0]
for count in [2,3,7,128]:
 for mode in range(6):
  for forward in [False,True]:
   for bit1 in [0,1]:
    for key in range(count):
     for terminal in [-1,key,(key+1)%count]:
      flags=(rng.getrandbits(32)&~0x2005)|(0x2000 if forward else 0)|bit1
      state=struct.pack('<2I2ifi',flags,mode,key,key,rng.choice([0.,.25,4.]),terminal)
      before=bytearray([0xa5]*1024)
      for i,offset in enumerate(offsets):before[offset:offset+4]=state[i*4:i*4+4]
      for offset in [0x2d8,0x2dc,0x2e0,0x2e4,0x31c,0x320,0x324,0x328]:struct.pack_into('<i',before,offset,-1)
      u.mem_write(base,bytes(before));u.mem_write(vector,struct.pack('<I',count));u.mem_write(stack,bytes(0x84));u.mem_write(stack+0x7c,struct.pack('<I',stop))
      u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,vector);u.reg_write(UC_X86_REG_EBP,1);u.reg_write(UC_X86_REG_ECX,flags);u.reg_write(UC_X86_REG_EDX,bit1)
      sounds.clear();u.emu_start(0x469da4,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
      after=bytes(u.mem_read(base,1024));result=b''.join(after[o:o+4] for o in offsets)
      for offset in offsets:before[offset:offset+4]=after[offset:offset+4]
      assert bytes(before)==after,('unexpected object mutation',count,mode,key)
      assert len(sounds)<=1;sound=sounds[0] if sounds else 0;sound_counts[sound]+=1
      cases.append(struct.pack('<I',count)+state);expected.append(struct.pack('<iI',0,sound)+result)
original_count=len(cases)
for count,mode,current,next_key,flags in [(1,1,0,0,0),(0x80000000,1,0,0,0),(2,6,0,0,0),(2,1,0,-1,0),(2,1,2,2,0),(2,1,1,0,0),(2,1,0,0,4)]:
 state=struct.pack('<2I2ifi',flags,mode,current,next_key,.5,-1);cases.append(struct.pack('<I',count)+state);expected.append(struct.pack('<iI',-4,0xa5a5a5a5)+state)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-arrive'],input=b''.join(cases));assert len(pc)==32*len(cases)
for n,want in enumerate(expected):assert pc[n*32:n*32+32]==want,(n,'PC',pc[n*32:n*32+32].hex(),want.hex())
x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'_rf_group_translation_arrive\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);x.mem_write(base+64,bytes([0xa5])*4);count,=struct.unpack_from('<I',wire);x.mem_write(stack,struct.pack('<4I',stop,base+4,count,base+64));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+64,4))+bytes(x.mem_read(base+4,24));assert got==expected[n],(n,'NXDK')
report=dict(result='PASS',original_cases=original_count,port_guards=7,sound_counts=dict(none=sound_counts[0],start=sound_counts[1],end=sound_counts[2]),scope='Original 469da4..46a02a, unchanged helpers with disabled sound handles; read-only call observers count sound requests. Every key in 2/3/7/128-key paths, all six modes, both directions, terminal and nonterminal paths, flag-1 states. Whole object mutation checked. PC/NXDK state and requests match. Caller must already apply arrival position, event/link effects and dwell decision; not full translation or audio playback.')
(root/'artifacts/group-arrival-verification.json').write_text(json.dumps(report,indent=2));print(report)
