"""Authored lower-door numeric trajectories; explicit empty occupancy/effect world."""
import json,struct,subprocess,runpy,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]
o=runpy.run_path(str(root/'tools/probe_group_translation.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(im)+4095)//4096*4096);x.mem_write(xb,im)
b=0x31000000;x.mem_map(b,65536);stack=b+50000;stop=b+64000
mapping=(root/'build/xbox/main.map').read_text()
entries={n:int(re.search('_rf_group_translation_tick_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for n in ('begin','move','finish')}
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
def invoke(n,args):
 x.mem_write(stack,w(stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entries[n],stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,n
def original_state():
 raw=bytes(o['u'].mem_read(o['base'],1024))
 return b''.join(raw[a:a+n] for a,n in [(0x318,4),(0x2e8,4),(0x2f8,8),(0x300,4),(0x30c,4),(0x2f4,4),(0x304,4),(0x310,4),(0x7c,4),(0xe4,12),(0xf0,12),(0x144,12)])
level=next(l for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if l['file'].lower()=='l1s1.rfl')
results=[]
for uid in (8593,8591):
 g=next(g for g in level['records'] if g['keys'][0]['uid']==uid)
 assert g['mode']==2 and g['flags']==[1,0,0,0,0,0] and len(g['keys'])==2
 o['setup'](timer=0);u=o['u'];base=o['base'];u.mem_write(base+0x2e8,w(2));u.mem_write(base+0x30c,w(-1));u.mem_write(base+0x314,w(-1));u.mem_write(base+0x318,w(0x80002002))
 for offset in (0x3c,0xe4,0xf0):u.mem_write(base+offset,struct.pack('<3f',*g['keys'][0]['position']))
 for i,k in enumerate(g['keys']):
  assert k['links']==[0xffffffff]*3
  u.mem_write(o['keys']+128*i+4,struct.pack('<3f',*k['position']));u.mem_write(o['keys']+128*i+0x34,struct.pack('<5f',*k['timing']))
 u.mem_write(0x5a4014,struct.pack('<f',1/60));runtime=original_state()
 wirekeys=b''.join(struct.pack('<8f',*k['position'],*k['timing']) for k in g['keys']);trace=[];commands=bytearray();expected=bytearray()
 for frame in range(420):
  now=1000+(frame+1)*1000//60;commands.extend(runtime+wirekeys+struct.pack('<fi',1/60,now));x.mem_write(b,runtime)
  for i,k in enumerate(g['keys']):
   key=bytearray(356);key[24:36]=struct.pack('<3f',*k['position']);key[72:92]=struct.pack('<5f',*k['timing']);x.mem_write(b+1024+356*i,bytes(key))
  x.mem_write(b+3000,w(0));invoke('begin',[b,b+1024,2,struct.unpack('<I',struct.pack('<f',1/60))[0],now,b+2048]);stage=struct.unpack('<I',x.mem_read(b+2120,4))[0]
  if stage==2:invoke('move',[b,b+2048]);stage=struct.unpack('<I',x.mem_read(b+2120,4))[0]
  if stage==3:invoke('finish',[b,b+2048,2,b+3000]);stage=struct.unpack('<I',x.mem_read(b+2120,4))[0]
  out=bytes(x.mem_read(b,76));requests=bytes(x.mem_read(b+3000,4));expected.extend(w(0)+out+requests+w(stage))
  u.mem_write(0x5a3ed8,w(now));o['call'](0x469800);assert out==original_state(),(uid,frame,'original tick')
  o['call'](0x46a8f0);runtime=bytearray(out);flags=struct.unpack_from('<I',runtime)[0]
  if flags&0x80000008:runtime[40:52]=runtime[52:64];struct.pack_into('<I',runtime,0,flags&~0x80000008)
  runtime=bytes(runtime);assert runtime==original_state(),(uid,frame,'commit')
  trace.append(dict(frame=frame+1,position=list(struct.unpack_from('<3f',runtime,40)),current=struct.unpack_from('<i',runtime,8)[0],next=struct.unpack_from('<i',runtime,12)[0],flags=struct.unpack_from('<I',runtime)[0]))
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-tick'],input=commands);assert pc==expected,(uid,'PC')
 first_move=next(t['frame'] for t in trace if t['position']!=g['keys'][0]['position'])
 first_open=next(t['frame'] for t in trace if t['position']==g['keys'][1]['position'])
 results.append(dict(uid=uid,first_move=first_move,first_open=first_open,trace=trace))
report=dict(result='PASS',ticks=840,scope='Authored lower-door positions/timings at60Hz, simultaneous already-active initial states. Exact original469800/46a8f0 versus PC/NXDK numeric runtime. Empty occupancy/obstruction world, source trigger absent, sounds disabled, key links absent as authored. Does not verify activation, hold-open/reversal, scene attachment or rendering.',results=results)
(root/'artifacts/authored-door-motion.json').write_text(json.dumps(report,indent=2)+'\n')
print(dict(result='PASS',ticks=840,doors=[{k:v for k,v in r.items() if k!='trace'} for r in results]))
