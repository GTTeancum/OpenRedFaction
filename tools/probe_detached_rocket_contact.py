"""Rocket-style nonsticky projectile against a kind3 extracted solid.
Derived from the existing weapons_object_contact probe; no desktop input.
"""
import hashlib,json,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_FPCW
SHA='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836';exe=R/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
im=pefile.PE(str(exe)).get_memory_mapped_image();B=0x30000000;W=B;C=B+0x4000;H=B+0x8000;HC=B+0xc000;S=B+0xe0000;STOP=B+0xf0000;FP=B+0xf1000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
I=(1,0,0,0,1,0,0,0,1);rows=[]
for mode in ('rocket',):
 for actor in (False,):
  u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000);u.mem_map(0,4096)
  word=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
  floats=lambda a,n:list(struct.unpack('<'+'f'*n,u.mem_read(a,4*n)))
  trace=[];services=[];handler=[];kind=5
  # Supplied numeric-service returns use fld/ret with the original x87 ABI.
  u.mem_write(FP,b'\xd9\x05'+w(FP+32)+b'\xc3');u.mem_write(FP+32,f(.5))
  u.mem_write(FP+16,b'\xd9\xee\xc3') # fldz; ret for recorded damage result
  def ret(value=0):
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp))
  def hook(cpu,a,n,d):
   sp=cpu.reg_read(UC_X86_REG_ESP)
   if a in (0x4c4b50,0x4c59f0,0x42cca0,0x4c5b5e,0x4c5b75,0x4c8b10,0x4c9b20,0x4c6301,0x48ab40):trace.append(hex(a))
   if a==0x4c4c3a:handler.append(cpu.reg_read(UC_X86_REG_EAX)&255)
   if a==0x40a0e0:
    value=word(sp+4);assert value in (123,456);trace.append('object:'+str(value));ret(H if value==456 else 0)
   elif a==0x426fc0:
    value=word(sp+4);assert value in (123,456);trace.append('entity:'+str(value));ret(H if value==456 and actor else 0)
   elif a==0x42ce00:
    assert [word(sp+4),word(sp+8),word(sp+12)]==[H,W,W+0x1b4];cpu.mem_write(word(sp+16),w(0));services.append(dict(service='location_scale',value=.5));cpu.reg_write(UC_X86_REG_EIP,FP)
   elif a==0x4892c0:
    args=[word(sp+4+i*4) for i in range(8)];assert args==[456,struct.unpack('<I',f(75 if actor else 400))[0],123,5,3,W+0x1b4,0xffffffff,0],args
    services.append(dict(service='direct_damage',target=args[0],amount=floats(sp+8,1)[0],owner=args[2],weapon=args[3],damage_kind=args[4],point=floats(args[5],3),auxiliary_uid=-1,force=args[7]));cpu.reg_write(UC_X86_REG_EIP,FP+16)
   elif a==0x488dc0:
    args=[word(sp+4+i*4) for i in range(5)];assert args==[W+0x1b4,struct.unpack('<I',f(400))[0],struct.unpack('<I',f(5))[0],123,3],args
    services.append(dict(service='radial_damage',point=floats(args[0],3),amount=floats(sp+8,1)[0],radius=floats(sp+12,1)[0],owner=args[3],kind=args[4]));ret()
   elif a==0x4c8a10:
    assert [word(sp+4),word(sp+12),word(sp+16),word(sp+20),word(sp+24)]==[5,W+0x3c,W+0x1b4,W+0x1c0,123];services.append(dict(service='impact_visual'));ret()
   elif a==0x434da0:services.append(dict(service='sound_lookup',token=word(sp+4)));ret(77)
   elif a==0x5056a0:assert word(sp+4)==77;services.append(dict(service='impact_sound'));ret()
   elif a==0x407fb0:
    assert [word(sp+4+i*4) for i in range(4)]==[H+0x2a0,123,0,1];services.append(dict(service='attached_actor_notification'));ret()
   elif a==0x40a490 and cpu.reg_read(UC_X86_REG_ECX)==W:services.append(dict(service='room'));ret(42)
   elif a in (0x467020,0x4c5820,0x4c69a0,0x4c8840,0x4c54b0,0x489fe0,0x5033b0):raise AssertionError(('excluded',mode,actor,hex(a)))
  u.hook_add(UC_HOOK_CODE,hook)
  for off,value in ((0x24,2),(0x2c,999),(0x30,123),(0x294,C),(0x298,kind),(0x2a8,0x10 if mode=='impact' else 0),(0x2b0,-1),(0x1e4,456),(0x1a8,0x80000879)):u.mem_write(W+off,w(value))
  u.mem_write(W+0x34,f(10));u.mem_write(W+0x29c,f(5));u.mem_write(W+0x1d4,f(1));u.mem_write(W+0x1b4,f(12,23,34));u.mem_write(W+0x1c0,f(0,0,1));u.mem_write(W+0x3c,f(11,22,33));u.mem_write(W+0x144,f(3,4,5))
  for off in (0x48,0xfc,0x120):u.mem_write(W+off,f(*I))
  u.mem_write(C+0x264,w(0x1000040 if mode=='remote' else 0));u.mem_write(C+0x108,f(400));u.mem_write(C+0x120,f(1));u.mem_write(C+0x208,f(5));u.mem_write(C+0xb0,f(20));u.mem_write(C+0x178,w(55));u.mem_write(C+0x52c,w(3))
  u.mem_write(H+0x24,w(0 if actor else 3));u.mem_write(H+0x2c,w(456));u.mem_write(H+0x200,w(-1));u.mem_write(H+0x294,w(HC));u.mem_write(H+0x34,f(100));u.mem_write(H+0x38,f(0));u.mem_write(H+0x3c,f(10,20,30));u.mem_write(H+0x48,f(*I))
  for a,value in ((0x872118,7),(0x872448,64),(0x87244c,-1),(0x87243c,-1),(0x7c7634,0),(0x7c75cc,0)):u.mem_write(a,w(value))
  u.mem_write(0x64ecb9,b'\0\0\0');u.mem_write(0x6fc4d8,b'\0');u.mem_write(S,w(STOP));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_ECX,W);u.reg_write(UC_X86_REG_FPCW,0x27f)
  u.emu_start(0x4c4b50,STOP,count=200000);assert u.reg_read(UC_X86_REG_EIP)==STOP and u.reg_read(UC_X86_REG_ESP)==S+4 and u.reg_read(UC_X86_REG_FPCW)==0x27f
  assert u.reg_read(UC_X86_REG_EAX)==0 and floats(W+0x34,1)==[10] and word(W+0x2f8)==8 and floats(W+0x2fc,3)==[12,23,34]
  assert floats(H+0x34,1)==[100] # recorded damage/radial services do not apply target mutations
  if mode=='remote':
   assert handler==[1] and not word(W+0x7c)&2 and word(W+0x2b0)==456 and floats(W+0x2b4,3)==[2,3,4] and floats(W+0x29c,1)==[20] and word(W+0x2a8)==0x40
   assert [x['service'] for x in services]==(['attached_actor_notification'] if actor else [])
  else:
   assert handler==[0] and word(W+0x7c)&2 and word(W+0x2b0)==0xffffffff and floats(W+0x29c,1)==[5]
   assert [x['service'] for x in services]==(['location_scale'] if actor else [])+['direct_damage','radial_damage','room','impact_visual','sound_lookup','impact_sound']
  prefix=['0x4c4b50','0x4c59f0','object:456','entity:456']+(['0x42cca0'] if actor else [])+['0x4c5b5e']
  expected=prefix+(['0x4c5b75','0x4c6301'] if mode=='remote' else ['0x4c8b10','object:123','entity:123','entity:456','0x4c9b20','0x4c8b10','object:123','0x4c6301','0x48ab40'])
  assert trace==expected,trace
  rows.append(dict(mode=mode,actor=actor,policy=0,handler_return=handler[0],life=floats(W+0x34,1)[0],fuse=floats(W+0x29c,1)[0],dead=bool(word(W+0x7c)&2),host=word(W+0x2b0),trace=trace,services=services))
out=R/'artifacts/geomod-postedit-re/rocket-solid-contact.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(dict(result='PASS',original_sha256=SHA,cases=rows,
 boundaries=['Full original projectile/object dispatcher with non-grenade nonsticky type5, kind3 target; damage400, radius5, explosive kind3 supplied from installed rocket table evidence.', 'Registry and presentation services supplied. Direct/radial requests recorded, not applied. Class flags0 exclude optional modifiers; no claim of complete authored rocket class reconstruction.']),indent=2)+'\n')
print('PASS',len(rows),'rocket/solid contact composition')
