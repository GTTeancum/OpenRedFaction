"""Execute the compiled NXDK pacing functions against an integer clock oracle."""
import json,re,struct,sys,random
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
p=pefile.PE(str(root/'build/xbox/main.exe'));raw=p.get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,raw)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
mapping=(root/'build/xbox/main.map').read_text()
def call(name,now):
 address=int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
 u.mem_write(stack,struct.pack('<3I',stop,base,now));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(address,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 return u.reg_read(UC_X86_REG_EAX)
rng=random.Random(601);n=0
for initial in [0,0xfffffff0]:
 u.mem_write(base,bytes(32));last=initial;credit=0;steps=draws=skips=discarded=0;started=False;now=initial
 for i in range(4000):
  now=(now+rng.choice([0,1,4,16,17,33,100,1000]))&0xffffffff
  if not started:credit=1000;started=True
  else:credit+=((now-last)&0xffffffff)*60
  last=now;discarded+=max(0,credit-8000);credit=min(credit,8000)
  expected=(1000-credit+59)//60 if credit<1000 else 0
  if not expected:credit-=1000;steps+=1
  assert call('rf_frame_clock_step',now)==expected
  if not expected:
   cost=rng.choice([0,1,8,16,17,50,100]);now=(now+cost)&0xffffffff
   credit+=cost*60;last=now;discarded+=max(0,credit-8000);credit=min(credit,8000)
   render=cost>=17 or not(credit>=1000 and skips<8)
   if render:skips=0;draws+=1
   else:skips+=1
   assert call('rf_frame_clock_present',now)==render
  assert bytes(u.mem_read(base,32))==struct.pack('<6IQ',1,last,credit,skips,steps,draws,discarded)
  n+=1
report={'result':'PASS','cases':n,'scope':'Compiled NXDK integer pacing matches oracle, including wrap, irregular intervals and long stalls; no emulator timing claim.'}
(root/'artifacts/frame-clock-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
