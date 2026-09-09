"""Connected Live Mines door ticks, mover propagation and commit vs original."""
import hashlib,json,struct,subprocess,sys,re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x400000);obj=base;controller=base+4096;keys=base+16384;arrays=base+32768;stack=base+0x300000;stop=base+0x3ff000
pose_fields=[(0x7c,4),(0x180,4),(0x238,12),(0x244,36),(0xe4,12),(0x3c,12),(0xf0,12),(0x144,12),(0x48,36),(0xfc,36),(0x120,36),(0x190,12),(0x19c,12)]
runtime_fields=[(0x318,4),(0x2e8,4),(0x2f8,8),(0x300,4),(0x30c,4),(0x2f4,4),(0x304,4),(0x310,4),(0x7c,4),(0xe4,12),(0xf0,12),(0x144,12)]
def map_in(blob,wire,fields):
 at=0
 for offset,size in fields:blob[offset:offset+size]=wire[at:at+size];at+=size
 assert at==len(wire)
def mapped(address,fields):return b''.join(bytes(u.mem_read(address+a,n)) for a,n in fields)
def call(address,arg):
 u.mem_write(stack,struct.pack('<2I',stop,arg));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f);u.emu_start(address,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop,hex(u.reg_read(UC_X86_REG_EIP))
p=pefile.PE(str(root/'build/xbox/main.exe'));xi=p.get_memory_mapped_image();xb=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);xbase=0x31000000;x.mem_map(xbase,65536);xs=xbase+50000;xe=xbase+64000
names=['rf_group_motion_activate','rf_group_translation_tick_begin','rf_group_translation_tick_move','rf_group_translation_tick_finish','rf_group_translation_bind_pose','rf_group_commit_positions']
mapping=(root/'build/xbox/main.map').read_text();entries={n:int(re.search('_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16) for n in names}
def invoke(name,args):
 x.mem_write(xs,struct.pack('<'+'I'*(len(args)+1),xe,*args));x.reg_write(UC_X86_REG_ESP,xs);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(entries[name],xe,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==xe and x.reg_read(UC_X86_REG_EAX)==0,name
probe=str(root/'build/pc/Release/rf_collision_probe.exe');archive=str(root/'Installed_Game/levels1.vpp');level='L1S1.rfl'
runtimes=subprocess.check_output([probe,'--runtime-groups',archive,level]);movers=subprocess.check_output([probe,'--bound-movers',archive,level]);inventory=next(l for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if l['file']==level)
mover_poses={struct.unpack_from('<i',movers,12+368*i)[0]:movers[144+368*i:380+368*i] for i in range(struct.unpack_from('<I',movers)[0])};results=[]
for gi,g in enumerate(inventory['records']):
 if g['flags'][1]:continue
 assert len(g['keys'])==2 and len(g['ids2'])==1 and not g['ids1'] and all(k['links']==[0xffffffff]*3 for k in g['keys'])
 start=16+320*gi;runtime=runtimes[start+8:start+84];cp=runtimes[start+84:start+320];mp=mover_poses[g['ids2'][0]];keywire=b''.join(struct.pack('<8f',*k['position'],*k['timing']) for k in g['keys'])
 pc=subprocess.check_output([probe,'--door-cycle'],input=runtime+cp+mp+keywire);assert len(pc)==40*552
 cb=bytearray(1024);mb=bytearray(1024);map_in(cb,cp,pose_fields);map_in(cb,runtime,runtime_fields);map_in(mb,mp,pose_fields)
 struct.pack_into('<I',mb,0x2c,0x12340000);struct.pack_into('<I',cb,0x2c,0x23450001);struct.pack_into('<I',cb,0x28c,0x64e3b0);struct.pack_into('<3I',cb,0x29c,2,2,arrays);struct.pack_into('<3I',cb,0x2cc,1,1,arrays+64)
 for offset in [0x2d8,0x2dc,0x2e0,0x2e4,0x314,0x31c,0x320,0x324,0x328]:struct.pack_into('<i',cb,offset,-1)
 u.mem_write(obj,bytes(mb));u.mem_write(controller,bytes(cb));u.mem_write(arrays,struct.pack('<2I',keys,keys+128));u.mem_write(arrays+64,struct.pack('<I',0x12340000));u.mem_write(0x7394cc,struct.pack('<2I',obj,controller));u.mem_write(0x64e63c,struct.pack('<I',controller));u.mem_write(0x64ecb9,bytes(2));u.mem_write(0x5a4014,struct.pack('<f',.25))
 for i,k in enumerate(g['keys']):
  kb=bytearray(128);struct.pack_into('<I',kb,0,k['uid']);kb[4:16]=keywire[32*i:32*i+12];kb[0x34:0x48]=keywire[32*i+12:32*i+32];struct.pack_into('<3I',kb,0x48,*k['links']);u.mem_write(keys+128*i,bytes(kb))
 u.mem_write(stack+0x340,struct.pack('<I',0xffffffff));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBX,controller);u.reg_write(UC_X86_REG_EDI,controller+0x29c)
 u.emu_start(0x46ac43,0x46acb2,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x46acb2
 x.mem_write(xbase,runtime);x.mem_write(xbase+256,cp);x.mem_write(xbase+512,mp)
 x.mem_write(xbase+1024,struct.pack('<6I',xbase,xbase+2048,xbase+1080,1,0,0));x.mem_write(xbase+1064,struct.pack('<2I',0x12340000,xbase+512));x.mem_write(xbase+1080,struct.pack('<I',0x12340000))
 for i in range(2):
  kb=bytearray(356);kb[24:36]=keywire[32*i:32*i+12];kb[72:92]=keywire[32*i+12:32*i+32];x.mem_write(xbase+2048+356*i,bytes(kb))
 invoke('rf_group_motion_activate',[xbase,2])
 moving=0
 for frame in range(40):
  u.mem_write(0x5a3ed8,struct.pack('<i',frame*250));call(0x469800,controller);call(0x46bbe0,0);call(0x46a8f0,controller)
  want=bytes(4)+mapped(controller,runtime_fields)+mapped(controller,pose_fields)+mapped(obj,pose_fields)
  got=pc[552*frame:552*(frame+1)];assert got==want,(g['name'],frame,[(i,got[i:i+4].hex(),want[i:i+4].hex()) for i in range(0,552,4) if got[i:i+4]!=want[i:i+4]])
  invoke('rf_group_translation_tick_begin',[xbase,xbase+2048,2,0x3e800000,frame*250,xbase+1200]);stage,=struct.unpack('<I',x.mem_read(xbase+1272,4))
  if stage==2:invoke('rf_group_translation_tick_move',[xbase,xbase+1200]);stage,=struct.unpack('<I',x.mem_read(xbase+1272,4))
  if stage==3:invoke('rf_group_translation_tick_finish',[xbase,xbase+1200,2,xbase+1400])
  x.mem_write(xbase+256,bytes(x.mem_read(xbase+36,4)));x.mem_write(xbase+256+80,bytes(x.mem_read(xbase+52,12)));x.mem_write(xbase+256+92,bytes(x.mem_read(xbase+64,12)))
  invoke('rf_group_translation_bind_pose',[xbase+512,0x12340000,xbase+1024,1,0x3e800000,0]);invoke('rf_group_commit_positions',[xbase,xbase+256,xbase+1024,xbase+1064,1])
  x.mem_write(xbase+40,bytes(x.mem_read(xbase+256+56,12)));x.mem_write(xbase+36,bytes(x.mem_read(xbase+256,4)))
  xbox=bytes(4)+bytes(x.mem_read(xbase,76))+bytes(x.mem_read(xbase+256,236))+bytes(x.mem_read(xbase+512,236));assert xbox==want,(g['name'],frame,'NXDK connected cycle')
  moving+=mapped(obj,pose_fields)[56:68]!=mp[56:68]
 results.append(dict(name=g['name'],mover_uid=g['ids2'][0],ticks=40,moved_frames=moving))
report=dict(result='PASS',doors=len(results),ticks=160,scope='Connected PC/NXDK activation, staged controller ticks, attached mover binding/propagation and position commits vs original activation block and complete unchanged 469800/46bbe0/46a8f0. Actual Live Mines key timings/positions/base mover poses. Gates unobstructed, sound handles disabled, door event links absent; crate rotation excluded. Compiled NXDK functions run in Unicorn; no XEMU motion or visual playback yet.',results=results)
(root/'artifacts/door-cycle-verification.json').write_text(json.dumps(report,indent=2));print(report)
