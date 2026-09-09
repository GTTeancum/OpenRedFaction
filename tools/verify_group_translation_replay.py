"""Compose reconstructed translation stages across complete original tick sequences."""
import json,re,runpy,struct,subprocess
from itertools import product
from pathlib import Path
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/probe_group_translation.py'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
import pefile
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(im)+4095)//4096*4096);x.mem_write(xb,im);base=0x31000000;x.mem_map(base,65536);stack=base+50000;stop=base+64000
mapping=(root/'build/xbox/main.map').read_text();entries={name:int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for name in ['rf_group_translation_integrate','rf_group_translation_position','rf_group_translation_arrive','rf_timer_expired','rf_timer_set']}
def invoke(name,args):
 x.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(entries[name],stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop;assert x.reg_read(UC_X86_REG_EAX)==0,name
def stage(mode,wire):
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-'+mode],input=wire);assert struct.unpack_from('<i',pc)[0]==0,(mode,pc.hex())
 x.mem_write(base,wire);x.mem_write(base+256,bytes([0xa5])*32)
 if mode=='integrate':invoke('rf_group_translation_integrate',[base,base+256]);out=bytes(x.mem_read(base+256,16))
 elif mode=='position':invoke('rf_group_translation_position',[base,base+56,base+72,base+260,base+256]);out=bytes(x.mem_read(base+256,16))
 else:
  count,=struct.unpack_from('<I',wire);invoke('rf_group_translation_arrive',[base+4,count,base+256]);out=bytes(x.mem_read(base+256,4))+bytes(x.mem_read(base+4,24))
 assert pc[4:]==out,(mode,'PC/NXDK');return out
def packed(s,mode,terminal):return struct.pack('<2I2ifi',s['flags'],mode,s['current'],s['next'],s['elapsed'],terminal)
traces=[];ticks=0
# Modes 1..5, both directions, duration/speed forms; run through several turns/wraps.
for mode in range(1,6):
 for reverse in [False,True]:
  for speed_mode,dwell in product([False,True],[0.,.5]):
   o['setup'](reverse,speed_mode=speed_mode,timer=0);o['u'].mem_write(o['base']+0x2e8,struct.pack('<I',mode));o['u'].mem_write(o['base']+0x30c,struct.pack('<i',-1))
   for i in range(2):o['u'].mem_write(o['keys']+128*i+0x34,struct.pack('<f',dwell))
   state=o['state']();terminal=-1;deadline=0;frames=[]
   for frame in range(40):
    before=dict(state);flags=before['flags'];arrival=0;sounds=0;now=1000+frame*250
    o['u'].mem_write(0x5a3ed8,struct.pack('<i',now))
    if before['next']!=-1:
     flags|=0x4008
     start=[8.*before['current'],0.,0.];end=[8.*before['next'],0.,0.]
     wire=struct.pack('<10fI3f',*start,*end,2.,0.,0.,.25,flags,before['speed'],before['elapsed'],before['distance'])
     progress=stage('integrate',wire);speed,elapsed,distance,length=struct.unpack('<4f',progress)
     invoke('rf_timer_expired',[deadline,now,base+256]);expired,=struct.unpack('<i',x.mem_read(base+256,4))
     state.update(flags=flags,speed=speed,elapsed=elapsed,distance=distance)
     if expired:
      pos=stage('position',wire+progress+struct.pack('<3f',*before['position']));arrival,=struct.unpack_from('<I',pos);pending=list(struct.unpack_from('<3f',pos,4));state['pending']=pending
     if arrival:
      state['speed']=0.;state['distance']=0.;state['current']=state['next']
      # Fixture key links are absent. Dwell precedes the arrival mode transition.
      if not state['flags']&1 and dwell>0:
       invoke('rf_timer_set',[base+256,now,int(dwell*1000+.5)]);deadline,=struct.unpack('<i',x.mem_read(base+256,4));state['flags']|=1;sounds=2
      else:
       output=stage('arrive',struct.pack('<I',2)+packed(state,mode,terminal));sounds,flags,m,current,next_key,phase,terminal=struct.unpack('<3I2ifi',output)
       state.update(flags=flags,current=current,next=next_key,elapsed=phase)
    o['call'](0x469800);actual=o['state']()
    assert state==actual,(mode,reverse,speed_mode,frame,'precommit',state,actual)
    actual_terminal,=struct.unpack('<i',o['u'].mem_read(o['base']+0x30c,4));assert terminal==actual_terminal
    actual_deadline,=struct.unpack('<i',o['u'].mem_read(o['base']+0x310,4));assert deadline==actual_deadline,(mode,frame,'deadline',deadline,actual_deadline)
    o['call'](0x46a8f0)
    if state['flags']&0x80000008:state['position']=list(state['pending']);state['flags']&=~0x80000008
    assert state==o['state'](),(mode,frame,'commit');ticks+=1
    if arrival:frames.append(dict(frame=frame,current=state['current'],next=state['next'],terminal=terminal,sound_requests=sounds))
   traces.append(dict(mode=mode,reverse=reverse,speed_mode=speed_mode,dwell=dwell,arrivals=frames,final=state))
report=dict(result='PASS',trajectories=len(traces),ticks=ticks,scope='Complete unchanged original translation and commit vs composed C integration, timer, position and arrival stages on PC/NXDK. Modes 1..5 both directions and timing modes over 40 ticks, absent key links, zero/half-second dwell and zero acceleration. Diagnostic composition supplies dwell decision, dirty flags and commit behavior; not a production full-tick API, trigger/event/rotation or attached-object propagation.',traces=traces)
(root/'artifacts/group-translation-replay.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='traces'})
