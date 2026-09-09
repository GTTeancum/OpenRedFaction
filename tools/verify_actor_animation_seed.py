"""Verify original constructor phase/generation fields against captured actor seed."""
import hashlib,json,struct,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
b=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000)
for fill in (0,0x5a,0xff):
 u.mem_write(base,bytes([fill])*0x2000);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(0x51af0b,0x51af84,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x51af84
 assert bytes(u.mem_read(base+0x1d04,4))==bytes(4)
 assert bytes(u.mem_read(base+0x1cf8,2))==b'\x01\x00'
 assert bytes(u.mem_read(base+0x12d0,4))==bytes(4)
report=dict(result='PASS',constructor_prefix_cases=3,scope='Original 51af0b..51af84 field initialization. Full constructor resource setup and initial actor selector inputs excluded.')
if len(sys.argv)>1:
 words=json.loads(Path(sys.argv[1]).read_text())['symbols']['rf_scene_actor_initial_animation']['words']
 assert words[:6]==[0,1,0,0xffffffff,0,0] and words[9:]==[1,320,0x3f800000]
 report['guest_seed_phase']=0;report['guest_seed_generation']=1;report['guest_first_slot_tick']=320
 report['guest_first_controller_self_blend']=False
(root/'artifacts/actor-animation-seed-verification.json').write_text(json.dumps(report,indent=2));print(report)
