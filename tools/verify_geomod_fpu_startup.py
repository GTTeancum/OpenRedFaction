"""Execute original CRT precision initializer5779e9 with complete controlfp callees."""
from pathlib import Path
exec((Path(__file__).parent/'verify_geomod_shallow_selection.py').read_text().split('configs=')[0])
rows=[]
for cw in [0x37f,0x27f,0x7f,0xf7f]:
 u.mem_write(S,w(stop));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,cw);u.emu_start(0x5779e9,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 after=u.reg_read(UC_X86_REG_FPCW);assert after==((cw&~0x340)|0x200);rows.append(dict(before=hex(cw),after=hex(after)))
(R/'artifacts/crater-shading-re/crater-fpu-startup.json').write_text(json.dumps(rows,indent=2));print('PASS:',rows)


