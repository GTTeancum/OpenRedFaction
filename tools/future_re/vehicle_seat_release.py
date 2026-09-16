"""Execute complete427380 seat release with audio/object lookup boundaries supplied."""
from pathlib import Path
import sys,itertools
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'local/python'))
(R/'artifacts/future-vehicles-re').mkdir(parents=True,exist_ok=True)
exec((R/'tools/verify_particle_render_states.py').read_text().split('xpe =')[0].replace('root = Path(__file__).resolve().parents[1]','root = R'))
from unicorn.x86_const import UC_X86_REG_ECX
host=base+0x1000;actor=base+0x3000;info=base+0x6000;array=base+0x7000;seats=[base+0x8000+i*32 for i in range(3)];events=[];exists=True
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
def cb(cpu,a,size,_):
 if a not in [0x40a0e0,0x434d00,0x5056a0]:return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda n:read(sp+n*4)
 if a==0x40a0e0:events.append(dict(kind='lookup',handle=arg(1),seats=[read(v+4) for v in seats]));val=actor if exists else 0
 elif a==0x434d00:events.append(dict(kind='sound_select',args=[arg(1),arg(2)]));val=777
 else:events.append(dict(kind='sound_play',args=[arg(i) for i in range(1,6)]));val=0
 cpu.reg_write(UC_X86_REG_EAX,val);cpu.reg_write(UC_X86_REG_EIP,arg(0));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,cb);rows=[]
for handles,request in [([-1,17,18],-1),([-1,17,18],99),([17,17,18],17),([-1,17,18],17)]:
 for exists,audio,player,enabled in itertools.product([False,True],repeat=4):
  events.clear();u.mem_write(host,bytes(0x2000));u.mem_write(actor,bytes(0x2000));u.mem_write(info,bytes(0x1000));word(host+0x294,info);word(info+0x118,123);word(host+0x8cc,3);word(host+0x8d4,array);u.mem_write(host+0x720,b'\x7f');word(actor+0x200,66);word(actor+0x1430,5 if player else 0);word(actor+0x858,0x12345678)
  u.mem_write(0x62fe70,bytes([int(enabled)]));word(0x630050,99)
  for i,(ptr,handle) in enumerate(zip(seats,handles)):word(array+i*4,ptr);word(ptr+4,handle&0xffffffff)
  u.mem_write(stack,struct.pack('<3I',stop,request&0xffffffff,int(audio)));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,host);u.emu_start(0x427380,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
  matched=request!=-1 and request in handles;success=matched and exists;expected=[v&0xffffffff for v in handles]
  if matched:expected[handles.index(request)]=0xffffffff
  after=[read(ptr+4) for ptr in seats];assert after==expected;assert u.reg_read(UC_X86_REG_EAX)&255==int(success);assert read(actor+0x200)==(0xffffffff if success else 66);assert u.mem_read(host+0x720,1)[0]==(0 if success else 0x7f)
  mode=read(actor+0x858);assert mode==((0x62fe70 if enabled else 0x62fe50) if success and player else 0x12345678)
  assert [e['kind'] for e in events]==((['lookup']+(['sound_select','sound_play'] if audio and exists else [])) if matched else [])
  rows.append(dict(handles=handles,request=request,object_exists=exists,audio=audio,player=player,run_enabled=enabled,success=success,seats_after=after,host_handle_after=read(actor+0x200),mode_pointer=hex(mode),events=events[:]))
(R/'artifacts/future-vehicles-re/seat-release.json').write_text(json.dumps(dict(scope=__doc__,exe_sha256=sha,cases=rows),indent=2)+'\n');print('PASS',len(rows),'full427380 cases; firstmatch,missinghandle,staleobject partialmutation,audio/player/movement fallback')
