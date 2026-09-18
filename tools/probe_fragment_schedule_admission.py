"""Execute complete488030 for settled/awake kind3 bodies, without hooks."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
SHA='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()==SHA
im=pefile.PE(str(exe)).get_memory_mapped_image();B=0x30000000;STACK=B+0xe000;STOP=B+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v))
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x10000)
rows=[]
for physics in [0,0x1800003f,0x8000003f,0x9800003f]:
 for flags in [0,0x6000000]:
  for visible in [0,1]:
   for parent in [-1,77]:
    u.mem_write(B,bytes(0x2000));u.mem_write(B,w(B+0x1000));u.mem_write(B+0x24,w(3));u.mem_write(B+0x34,struct.pack('<f',50))
    u.mem_write(B+0x7c,w(flags));u.mem_write(B+0x1a8,w(physics));u.mem_write(B+0x200,w(parent));u.mem_write(B+0x1160,bytes([visible]))
    u.mem_write(0x7c7634,w(0));before=bytes(u.mem_read(B,0x2000))
    u.mem_write(STACK,w(STOP,B));u.reg_write(UC_X86_REG_ESP,STACK);u.emu_start(0x488030,STOP,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==STOP
    admitted=u.reg_read(UC_X86_REG_EAX)&255;want=bool(physics&0x80000000) and parent==-1 and bool(visible)
    assert admitted==want,(hex(physics),hex(flags),visible,parent,admitted)
    expected=bytearray(before);expected[0x7c:0x80]=w(flags if want else flags|0x800000)
    assert bytes(u.mem_read(B,0x2000))==expected
    rows.append(dict(physics_flags=physics,object_flags=flags,room_visible=visible,parent=parent,admitted=bool(admitted)))
out=ROOT/'artifacts/geomod-postedit-re/fragment-schedule-admission.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(dict(result='PASS',original_sha256=SHA,cases=rows,scope='Complete488030 and real helpers with healthy kind3, supplied room visibility and no active player associations. No world step, mover query, wake producer or scene execution.'),indent=2)+'\n')
print('PASS',len(rows),'original fragment schedule-admission cases')
