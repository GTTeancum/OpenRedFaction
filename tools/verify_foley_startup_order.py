"""Original startup sound/Foley/entity-loader ordering at explicit boundaries."""
import hashlib,json,struct,sys
from pathlib import Path
import capstone,pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
path=root/'Installed_Game/RF.exe';digest=hashlib.sha256(path.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x10000);stack=base+0x8000
start,stop=0x4b22f4,0x4b2366
instructions=list(capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32).disasm(p.get_data(start-0x400000,stop-start),start))
assert instructions[-1].address==0x4b2361 and instructions[-1].mnemonic=='call' and instructions[-1].op_str=='0x41b730'
# Only supply callees at the selected block's direct-call boundaries. Execute
# 4346f0 and its real506270 gate, but intercept table loads/device-independent
# finalizer/exit-registration rather than initializing the entire game.
boundaries={int(i.op_str,16) for i in instructions if i.mnemonic=='call'}-{0x4346f0}
boundaries|={0x434720,0x434880,0x505480,0x5735cd}
trace=[]
def hook(uc,address,size,context):
 if address not in boundaries:return
 trace.append(address);sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0]
 if address==0x5735cd:assert struct.unpack('<I',uc.mem_read(sp+4,4))[0]==0x434c80
 uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rows=[]
for gate in (0,1,255):
 trace=[];u.mem_write(0x17543d8,bytes([gate]));u.reg_write(UC_X86_REG_ESP,stack)
 u.emu_start(start,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
 selected=[a for a in trace if a in (0x434720,0x434880,0x505480,0x5735cd,0x41b730)]
 assert selected==([0x434720] if gate else [])+[0x434880,0x505480,0x5735cd,0x41b730]
 rows.append(dict(audio_gate=gate,calls=[hex(a) for a in selected]))
report=dict(result='PASS',original_sha256=digest,cases=rows,scope='Original4b22f4..4b2366 and4346f0 with actual506270 gate. Intercept all other direct callees and table/finalizer/exit-registration boundaries. Confirms sounds.tbl (when enabled), Foley, then entity-class initialization order in this startup segment. Does not execute loader bodies, later level paths or registry reset/lifetime.')
(root/'artifacts/foley-startup-order.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
