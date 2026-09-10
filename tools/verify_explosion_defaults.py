"""Original explosion recipe defaults and shared central random-position field.
Parser, string-copy and emitter-name lookup results are supplied explicitly.
"""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b);base=0x30000000;u.mem_map(base,65536);stack=base+0xe000;stop=base+0xf000;target=0x75e520
u.mem_write(base+0x2100,b'\xd9\x05'+struct.pack('<I',base+0x2000)+b'\xc3')
def word(a):return struct.unpack('<I',u.mem_read(a,4))[0]
def write(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
def text(a):
 s=bytearray()
 while u.mem_read(a,1)[0]:s+=u.mem_read(a,1);a+=1
 return s.decode().lower()
central=-1;label='';mask=0;count=0
optional=['+min_size_before_use:','+play_time_factor:','+rand_pos_factor:','$sparks_emitter:','$trail_head_emitter:','$trail_tail_emitter:']
def hook(m,address,size,unused):
 global label,central
 pops={0x511fc0:8,0x5126a0:4,0x5125c0:4,0x512c20:16,0x513020:8,0x512920:0,0x5126f0:0,0x512750:0,0x497550:0}
 if address not in pops:return
 sp=m.reg_read(UC_X86_REG_ESP);ret=word(sp);value=0
 if address in (0x511fc0,0x5126a0,0x5125c0):
  label=text(word(sp+4))
  if label=='+central_emitter:':central+=1;value=int(central<count)
  elif address!=0x5125c0:value=1
  elif label in optional:value=int(bool(mask&(1<<optional.index(label))))
  else:raise AssertionError(label)
 elif address==0x512c20:u.mem_write(word(sp+4),b'fixture\0')
 elif address==0x513020:value=1
 elif address==0x497550:value=central+10
 elif address==0x5126f0:value=(central%2)+1
 elif address==0x512750:value=37
 elif address==0x512920:
  # Unique per central slot to reveal accidental per-entry indexing.
  f=2.5 if label=='$explosion_play_time:' else 3.25+central
  u.mem_write(base+0x2000,struct.pack('<f',f));m.reg_write(UC_X86_REG_EIP,base+0x2100);return
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4+pops[address]);m.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook);cases=0
for count in (0,1,2,6):
 for mask in range(64):
  central=-1;u.mem_write(target,bytes([0xa5])*152);write(0x75ec44,0);write(stack,stop);u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x48dd90,stop,count=100000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==0
  assert word(target+0x20)==1 and word(target+0x7c)==count
  assert bytes(u.mem_read(target+0x88,4))==struct.pack('<f',2.5)
  for slot in range(count):
   assert word(target+0x2c+slot*4)==slot+10
   assert u.mem_read(target+0x44+slot,1)[0]==slot%2+1
   assert bytes(u.mem_read(target+0x4c+slot*4,4))==(struct.pack('<f',3.25+slot) if mask&1 else bytes(4))
   assert bytes(u.mem_read(target+0x64+slot*4,4))==(struct.pack('<f',3.25+slot) if mask&2 else struct.pack('<I',0x7f7fffff))
  assert bytes(u.mem_read(target+0x90,4))==(struct.pack('<f',2.25+count) if mask&4 else bytes(4)) if count else word(target+0x90)==0xa5a5a5a5
  assert word(target+0x80)==(count+10 if mask&8 else 0xffffffff)
  assert word(target+0x84)==(37 if mask&8 else 0)
  assert word(target+0x24)==(count+10 if mask&16 else 0xffffffff)
  assert word(target+0x28)==(count+10 if mask&32 else 0xffffffff)
  assert bytes(u.mem_read(target+0x8c,4))==(struct.pack('<f',3.25+count) if mask&16 else bytes([0xa5])*4)
  assert bytes(u.mem_read(target+0x94,4))==(struct.pack('<f',3.25+count) if mask&4 else bytes(4)) if mask&16 else word(target+0x94)==0xa5a5a5a5
  cases+=1
report=dict(result='PASS',cases=cases,original_sha256=sha,scope='Original 48dd90 with supplied parser/string/emitter-resolution seams. 0/1/2/6 central slots, all 64 optional masks, untouched absent-trail fields, raw boolean byte preservation, shared last-central random-position factor. No actual parser, missing-emitter fatal branch or explosion execution.',layout=dict(stride=152,central_handles=44,central_process_bytes=68,central_min_sizes=76,central_play_factors=100,central_count=124,sparks_handle=128,sparks_number=132,play_time=136,head_time=140,central_rand_shared=144,head_rand=148))
(root/'artifacts/explosion-defaults-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)

