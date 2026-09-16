"""CSG4de27c..4de2a8 executes actual solid bounds rebuild with supplied vertex container/room refresh."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
def bounds_hook(cpu,a,size,_):
 if a not in [0x40a490,0x40a480,0x4ccf50]:return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if a!=0x4ccf50:assert cpu.reg_read(UC_X86_REG_ECX)==B+0x78
 cpu.reg_write(UC_X86_REG_EAX,2 if a==0x40a490 else B+0x1000+rd(sp+4)*4 if a==0x40a480 else 0);cpu.reg_write(UC_X86_REG_EIP,rd(sp));cpu.reg_write(UC_X86_REG_ESP,sp+(8 if a==0x40a480 else 4))
u.hook_add(UC_HOOK_CODE,bounds_hook);u.mem_write(B+0x48,f(-1,-1,-1,1,1,1));u.mem_write(B+0x1000,w(B+0x2000,B+0x2010));u.mem_write(B+0x2000,f(-2,-3,-4));u.mem_write(B+0x2010,f(5,6,7));u.mem_write(0x6460e8,w(B));u.mem_write(0x648600,bytes(range(32)));before=bytes(u.mem_read(0x648600,32));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_EDI,B);u.reg_write(UC_X86_REG_EAX,B+0x5000);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x4de27c,0x4de2a8,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4de2a8
out=struct.unpack('<6f',u.mem_read(B+0x48,24));assert out[0]<-2 and out[3]>5;assert bytes(u.mem_read(0x648600,32))==before
(R/'artifacts/crater-shading-re/crater-bounds-lifetime.json').write_text(json.dumps(dict(previous_bounds=[-1,-1,-1,1,1,1],new_bounds=out,packed_record_unchanged=True),indent=2));print('PASS:',out,'packed record unchanged')
