"""Original467275..467375 admission writes, real packing/copies; queue boundary captured and436fc0 copy executed separately."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
queued=[]
def capture(cpu,a,size,_):
 if a==0x437230:
  sp=cpu.reg_read(UC_X86_REG_ESP);queued.append(bytes(cpu.mem_read(rd(sp+4),100)));cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,capture);rows=[]
for requested,adjusted in [((0,0,0),(0,1,0)),((1,2,3),(4,5,6)),((-2,0,1),(1,1,1)),((10,10,10),(10,10,10))]:
 u.mem_write(B,bytes(0x10000));u.mem_write(B+0x48,f(-32768,-32768,-32768,32768,32768,32768));u.mem_write(0x6460e8,w(B));u.mem_write(0x647c9c,w(0));u.mem_write(B+0x200,f(*requested));u.mem_write(S,bytes(0x200));u.mem_write(S+0x24,w(7,11)+f(*adjusted,1,0,0,0,1,0,0,0,1)+w(0)+f(1,0,1,0,0,-2,0,-3,0,0));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EBP,B+0x200);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x467275,0x467375,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x467375
 packed=struct.unpack('<3H',u.mem_read(0x648608,6));aux=struct.unpack('<9f',u.mem_read(0x646a28,36));qcenter=struct.unpack('<3f',queued[-1][8:20]);assert qcenter==adjusted and aux[:3]==adjusted;assert packed==tuple(int(x+32768) for x in requested);assert rd(0x647c9c)==1
 u.mem_write(B+0x300,queued[-1]);u.mem_write(S,w(stop,B+0x300));u.reg_write(UC_X86_REG_ECX,B+0x400);u.reg_write(UC_X86_REG_ESP,S);u.emu_start(0x436fc0,stop,count=10000);assert bytes(u.mem_read(B+0x400,100))==queued[-1]
 rows.append(dict(requested=requested,adjusted=adjusted,queue_center=qcenter,packed_record_center=packed,auxiliary=aux,queue_copy_exact=True))
(R/'artifacts/crater-shading-re/crater-center-ownership.json').write_text(json.dumps(rows,indent=2));print('PASS:',len(rows),'admission writes and complete queue copies')
