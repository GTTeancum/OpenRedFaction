"""Staged C translation runtime against complete original tick/commit sequences."""
import json,re,runpy,struct,subprocess
from itertools import product
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_translation.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(im)+4095)//4096*4096);x.mem_write(xb,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
mapping=(root/'build/xbox/main.map').read_text();entries={name:int(re.search('_rf_group_translation_tick_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ['begin','move','finish']}
def invoke(name,args):
 x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(entries[name],stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop;assert x.reg_read(UC_X86_REG_EAX)==0,name
def original_state():
 b=bytes(o['u'].mem_read(o['base'],1024));return b''.join(b[a:a+n] for a,n in [(0x318,4),(0x2e8,4),(0x2f8,8),(0x300,4),(0x30c,4),(0x2f4,4),(0x304,4),(0x310,4),(0x7c,4),(0xe4,12),(0xf0,12),(0x144,12)])
sounds=[]
def observe(uc,address,size,data):sounds.append(data)
o['u'].hook_add(UC_HOOK_CODE,observe,user_data=1,begin=0x46a120,end=0x46a120)
o['u'].hook_add(UC_HOOK_CODE,observe,user_data=2,begin=0x46a0d0,end=0x46a0d0)
o['u'].ctl_flush_tb() # Baseline replay ran before observers were installed.
ticks=0;stage_counts={};sound_counts={};traces=[]
for mode,reverse,speed_mode,dwell in product(range(1,6),[False,True],[False,True],[0.,.5]):
 o['setup'](reverse,speed_mode=speed_mode,timer=0);o['u'].mem_write(o['base']+0x2e8,struct.pack('<I',mode));o['u'].mem_write(o['base']+0x30c,struct.pack('<i',-1))
 for i in range(2):o['u'].mem_write(o['keys']+128*i+0x34,struct.pack('<f',dwell))
 runtime=original_state();assert len(runtime)==76
 keywire=b''.join(struct.pack('<8f',8.*i,0.,0.,dwell,2.,2.,0.,0.) for i in range(2))
 for frame in range(40):
  now=1000+250*frame;wire=runtime+keywire+struct.pack('<fi',.25,now)
  pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-tick'],input=wire);assert len(pc)==88 and struct.unpack_from('<i',pc)[0]==0
  x.mem_write(base,runtime)
  for i in range(2):
   key=bytearray(356);key[24:36]=keywire[32*i:32*i+12];key[72:92]=keywire[32*i+12:32*i+32];x.mem_write(base+1024+356*i,bytes(key))
  x.mem_write(base+3000,bytes(4));invoke('begin',[base,base+1024,2,0x3e800000,now,base+2048]);stage,=struct.unpack('<I',x.mem_read(base+2048+72,4))
  if stage==2:invoke('move',[base,base+2048]);stage,=struct.unpack('<I',x.mem_read(base+2048+72,4))
  if stage==3:invoke('finish',[base,base+2048,2,base+3000]);stage,=struct.unpack('<I',x.mem_read(base+2048+72,4))
  out=bytes(x.mem_read(base,76));requests,=struct.unpack('<I',x.mem_read(base+3000,4));assert pc[4:]==out+struct.pack('<2I',requests,stage),(mode,frame,'PC/NXDK')
  o['u'].mem_write(0x5a3ed8,struct.pack('<i',now));sounds.clear();o['call'](0x469800)
  assert out==original_state(),(mode,reverse,speed_mode,dwell,frame,'original runtime',out.hex(),original_state().hex())
  assert sounds==([requests] if requests else []),(mode,frame,'sounds',sounds,requests)
  stage_counts[stage]=stage_counts.get(stage,0)+1;sound_counts[requests]=sound_counts.get(requests,0)+1
  o['call'](0x46a8f0);runtime=bytearray(out);flags,=struct.unpack_from('<I',runtime)
  if flags&0x80000008:runtime[40:52]=runtime[52:64];struct.pack_into('<I',runtime,0,flags&~0x80000008)
  runtime=bytes(runtime);assert runtime==original_state(),(mode,frame,'commit');ticks+=1
 traces.append(dict(mode=mode,reverse=reverse,speed_mode=speed_mode,dwell=dwell,final_runtime=runtime.hex()))
report=dict(result='PASS',trajectories=len(traces),ticks=ticks,stages=stage_counts,sounds=sound_counts,scope='Staged C begin/move/finish runs on PC/NXDK against complete original ticks, timer deadlines, sound-call requests and mapped runtime bytes. Modes 1..5, both directions/timing forms, zero/half-second dwell. External gates pass, event links absent and sounds disabled; original pending commit compared to diagnostic assignment. Not scene binding or recovered trigger/event/rotation behavior.',traces=traces)
(root/'artifacts/group-runtime-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='traces'})
