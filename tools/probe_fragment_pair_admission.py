"""Execute complete original48be00 pair rejection for extracted terrain bodies."""
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
for other_kind in [3,4]:
 for flags_a in [0x8000003f,0x1800003f,0xc000003f,0]:
  for flags_b in [0x8000003f,0x1800003f,0xc000003f,0]:
   for radius in [.1,.5,3,10]:
    u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
    for obj,kind,flags in [(B,3,flags_a),(B+0x1000,other_kind,flags_b)]:
     u.mem_write(obj+0x24,w(kind));u.mem_write(obj+0x1a8,w(flags));u.mem_write(obj+0x180,f(radius));u.mem_write(obj+0x190,f(-10,-10,-10,10,10,10))
    u.mem_write(0x6fc4d8,b'\0');u.mem_write(0x64ecb9,b'\0');u.mem_write(B+0x3000,w(0));before=bytes(u.mem_read(B,0x2000))
    u.mem_write(S,w(STOP,B,B+0x1000,B+0x3000));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f)
    u.emu_start(0x48be00,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP
    rejected=u.reg_read(UC_X86_REG_EAX)&255;pair_flags=struct.unpack('<I',u.mem_read(B+0x3000,4))[0]
    assert rejected==int(other_kind==3 or not((flags_a|flags_b)&0x20)),(other_kind,flags_a,flags_b,radius,rejected)
    assert bytes(u.mem_read(B,0x2000))==before and pair_flags==0
    rows.append(dict(kinds=[3,other_kind],physics_flags=[flags_a,flags_b],radius=radius,rejected=bool(rejected),pair_flags=pair_flags))
report=dict(result='PASS',original_sha256=sha,cases=rows,scope='Complete48be00 with no hooks or substituted callees. Overlapping synthetic bounds; healthy visible single-player bodies. Kind3/3 is always rejected, while kind3/4 positive controls pass when either body has physics bit20. Pair-allocation caller48bd80 interprets return1 as reject, confirmed by instruction inspection. No claim of all entity-type policies, native gameplay or pair-contact geometry.')
(ROOT/'artifacts/geomod-postedit-re/fragment-pair-admission.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(rows),'pair-admission cases;',sum(not r['rejected'] for r in rows),'positive controls')
