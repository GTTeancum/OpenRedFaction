"""Particle frame selection against original 494baf..494c8f and CRT callees."""
import runpy,struct,re,itertools,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u=c['u'];x=c['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EAX
u.hook_add(UC_HOOK_CODE,lambda m,a,size,data:m.emu_stop(),begin=0x494c8f,end=0x494c8f)
entry=int(re.search(r'_rf_particle_frame_index\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
commands=bytearray();expected=bytearray();cases=0
for count,age,life,finish,flags,secondary in itertools.product((0,1,2,4,16,30,64,32767,32768,65535),(0,0.03125,0.5,0.9999999403953552,1,1.5,2,10),(0.5,1,3),(0.25,1,2),(0,256),(0,4)):
 p=bytearray(120);struct.pack_into('<f',p,0x24,age);struct.pack_into('<f',p,0x34,life);struct.pack_into('<IHH',p,0x48,1000,count,secondary);struct.pack_into('<If',p,0x58,flags,finish)
 u.mem_write(base,bytes(p));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.emu_start(0x494baf,0x494c8f,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x494c8f
 frame=u.reg_read(UC_X86_REG_EDI)-1000;result=struct.pack('<iI',0,frame);commands.extend(p);expected.extend(result)
 x.mem_write(base,bytes(p));x.mem_write(base+0x100,bytes([0xa5])*4);x.mem_write(stack,struct.pack('<3I',stop,base,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,4))==result,(count,age,life,finish,flags,secondary,frame)
 cases+=1
rejections=0
for offset,value,hold in ((0x24,-1,0),(0x24,float('nan'),0),(0x24,1e30,0),(0x34,0,0),(0x34,float('inf'),0),(0x5c,0,4)):
 p=bytearray(120);struct.pack_into('<f',p,0x24,1);struct.pack_into('<f',p,0x34,1);struct.pack_into('<HH',p,0x4c,16,hold);struct.pack_into('<f',p,0x5c,1);struct.pack_into('<f',p,offset,value)
 result=struct.pack('<iI',-4,0xa5a5a5a5);commands.extend(p);expected.extend(result)
 x.mem_write(base,bytes(p));x.mem_write(base+0x100,bytes([0xa5])*4);x.mem_write(stack,struct.pack('<3I',stop,base,base+0x100));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
 assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x100,4))==result
 rejections+=1
assert subprocess.check_output([str(c['probe']),'--particle-frame'],input=commands)==expected
report=dict(result='PASS',cases=cases,rejections=rejections,x87_control_word='0x027f',scope='Original frame-selection span and unchanged CRT floor/conversion callees vs shared PC/NXDK; normal, loop and hold-last modes, signed frame counts and boundaries. Bitmap binding/drawing excluded.')
(root/'artifacts/particle-frame-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
