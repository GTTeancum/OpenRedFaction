"""Original48be00 actor/fragment admission, both object orderings, no hooks."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image();B=0x30000000;S=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
rows=[]
for reverse in [0,1]:
 for player in [0,1]:
  for mode in [0,1,3]:
   for mesh in [0,1]:
    for radius in [.49,.5,.5001,1,1.0001,3]:
     u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
     u.mem_write(B+0x24,w(3));u.mem_write(B+0x180,f(radius));u.mem_write(B+0x294,w(B+0x4000 if mesh else 0));u.mem_write(B+0x1a8,w(0x8000003f))
     u.mem_write(B+0x1024,w(0));u.mem_write(B+0x107c,w(8 if player else 0));u.mem_write(B+0x11a8,w(0x8000003f));u.mem_write(B+0x1294,w(B+0x2000));u.mem_write(B+0x21b4,w(mode))
     u.mem_write(0x6fc4d8,b'\0');u.mem_write(0x64ecb9,b'\0');u.mem_write(B+0x3000,w(0));before=bytes(u.mem_read(B,0x2800))
     pair=[B,B+0x1000];pair=pair[::-1] if reverse else pair
     u.mem_write(S,w(STOP,*pair,B+0x3000));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x48be00,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
     rejected=bool(u.reg_read(UC_X86_REG_EAX)&255);flags=struct.unpack('<I',u.mem_read(B+0x3000,4))[0]
     expected=radius>.5 and ((player and mesh) or mode==1)
     assert rejected== (not expected),(reverse,player,mode,mesh,radius,rejected)
     expected_flags=(16 if reverse else 8) if player and mesh and radius>1 else 0
     assert flags==expected_flags,(reverse,player,mode,mesh,radius,flags)
     assert bytes(u.mem_read(B,0x2800))==before
     rows.append(dict(reverse=reverse,player=player,use_kind=mode,mesh=mesh,radius=radius,rejected=rejected,pair_flags=flags))
report=dict(result='PASS',original_sha256=sha,cases=rows,scope='Complete48be00 with real4895d0/429990/486c90, no hooks. Other common rejection flags clear; single player. Actor class physics use-kind and fragment body radius are explicit inputs. Geometry queries and live NPC movement are not executed.')
(ROOT/'artifacts/geomod-postedit-re/fragment-actor-admission.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(rows),'actor/fragment admission cases')
