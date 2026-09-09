"""Translation pending position vs original block, using verified integration fixtures."""
import json,re,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];o=runpy.run_path(str(root/'tools/verify_group_integration.py'))
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
u=o['machine'](root/'Installed_Game/RF.exe');base=0x30000000;keys=base+4096;array=base+8192;stack=base+50000;stop=base+64000
def reached(uc,address,size,data):uc.emu_stop()
u.hook_add(UC_HOOK_CODE,reached,begin=0x469cf7,end=0x469cf7)
cases=[];expected=[];arrivals=snaps=moves=0
for n,(step,integrated) in enumerate(zip(o['cases'][:10100],o['expected'][:10100])):
 if struct.unpack_from('<i',integrated)[0]:continue
 step=bytearray(step);flags,=struct.unpack_from('<I',step,40)
 if n%7==0:flags|=1
 struct.pack_into('<I',step,40,flags);v=struct.unpack('<10fI3f',step);progress=integrated[4:];speed,elapsed,distance,length=struct.unpack('<4f',progress)
 position=struct.pack('<3f',v[0]+.25,v[1]-.5,v[2]+1)
 before=bytearray([0xa5]*1024);before[0xe4:0xf0]=position
 struct.pack_into('<3I',before,0x29c,2,2,array);struct.pack_into('<I',before,0x2e8,1);struct.pack_into('<2i',before,0x2f8,0,1);struct.pack_into('<I',before,0x318,flags);struct.pack_into('<f',before,0x2f4,speed);struct.pack_into('<f',before,0x304,distance)
 u.mem_write(base,bytes(before));u.mem_write(array,struct.pack('<2I',keys,keys+128))
 for i in range(2):u.mem_write(keys+i*128,bytes(4)+bytes(step[12*i:12*i+12])+bytes(112))
 u.mem_write(stack,bytes(0x84));u.mem_write(stack+0x7c,struct.pack('<I',stop));u.mem_write(stack+0x10,struct.pack('<3f',length,v[6],v[13]));u.mem_write(0x5a4014,bytes(step[36:40]))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EDI,base+0x29c);u.reg_write(UC_X86_REG_EBP,base+0x2f4);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x469b59,stop,count=10000);ip=u.reg_read(UC_X86_REG_EIP);assert ip in [stop,0x469cf7]
 due=int(ip==0x469cf7);after=bytes(u.mem_read(base,1024));pending=after[0xf0:0xfc];before[0xf0:0xfc]=pending;assert bytes(before)==after,(n,'object mutation')
 arrivals+=due
 if not due:
  if distance>=length:snaps+=1
  else:moves+=1
 cases.append(bytes(step)+progress+position);expected.append(struct.pack('<iI',0,due)+pending)
for offset in [0,56,72]:
 wire=bytearray(cases[0]);struct.pack_into('<I',wire,offset,0x7fc00000);cases.append(bytes(wire));expected.append(struct.pack('<i',-2)+bytes([0xa5])*16)
pc=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--group-position'],input=b''.join(cases));assert len(pc)==20*len(cases)
for n,want in enumerate(expected):assert pc[n*20:n*20+20]==want,(n,'PC',pc[n*20:n*20+20].hex(),want.hex())
x=o['machine'](root/'build/xbox/main.exe');entry=int(re.search(r'_rf_group_translation_position\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for n,wire in enumerate(cases):
 x.mem_write(base,wire);x.mem_write(base+128,bytes([0xa5])*16);x.mem_write(stack,struct.pack('<6I',stop,base,base+56,base+72,base+132,base+128));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+128,16));assert got==expected[n],(n,'NXDK',got.hex(),expected[n].hex())
report=dict(result='PASS',original_cases=len(cases)-3,port_guards=3,arrivals=arrivals,crossing_snaps=snaps,intermediate_moves=moves,scope='Original position block with unchanged vector helpers and mode 1 to exclude obstruction reversal; stop before arrival effects. Integration-derived finite fixtures, flag-1 arrival cases. Whole object writes and exact pending/arrival match PC/NXDK. No full tick, trigger, dwell, or attached-object propagation.')
(root/'artifacts/group-position-verification.json').write_text(json.dumps(report,indent=2));print(report)

