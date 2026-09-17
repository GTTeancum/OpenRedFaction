"""Execute original terminal-fragment lifetime setup and lifecycle, without game UI."""
import hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_FPCW
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
im=pefile.PE(str(exe)).get_memory_mapped_image()
w=lambda *x:struct.pack('<'+'I'*len(x),*(v&0xffffffff for v in x))
f=lambda *x:struct.pack('<'+'f'*len(x),*x)
B=0x30000000;S=B+0xe0000;STOP=B+0xf0000;rows=[]
for radius in [.1,1.5,3,3.0001,10]:
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0,0x1000);u.mem_map(0x400000,(len(im)+4095)&~4095);u.mem_write(0x400000,im);u.mem_map(B,0x100000)
 captured=[]
 def word(a):return struct.unpack('<I',u.mem_read(a,4))[0]
 def ret(value=0,pop=0):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_EIP,word(sp));u.reg_write(UC_X86_REG_ESP,sp+4+pop)
 def hook(cpu,a,n,d):
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x4130b0:
   assert word(sp+4)==0xffffffff and word(sp+8)==B
   captured.append(bytes(cpu.mem_read(word(sp+12),0x64)));ret()
  elif a in (0x4ff3b0,0x4ff480,0x4ff470,0x4689a0):ret(0)
  elif a==0x4ffa80:ret(0,4)
 u.hook_add(UC_HOOK_CODE,hook)
 u.mem_write(B+0x60,f(radius));u.mem_write(B+0x1000,f(1,2,3));u.mem_write(S,w(STOP,B,B+0x1000));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f)
 u.emu_start(0x466440,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP and len(captured)==1
 desc=captured[0];delay=struct.unpack_from('<i',desc,0x48)[0];assert delay==-1
 # Execute actual constructor lifetime-selection span, including timer clear.
 u.mem_write(B+0x2000,desc);u.mem_write(B+0x3000,bytes(0x400));u.reg_write(UC_X86_REG_ESI,B+0x3000);u.reg_write(UC_X86_REG_EDI,B+0x2000);u.reg_write(UC_X86_REG_ESP,S)
 u.emu_start(0x4131db,0x413215,count=10000);assert word(B+0x32a4)==0xffffffff
 life=[]
 for now in [0,1000,60000,1072800000]:
  for health in [50,0,-1]:
   u.mem_write(B+0x3034,f(health));u.mem_write(B+0x307c,w(0));u.mem_write(0x5a3ed8,w(now));u.mem_write(S,w(STOP,B+0x3000));u.reg_write(UC_X86_REG_ESP,S)
   before=bytes(u.mem_read(B+0x3000,0x400));u.emu_start(0x412ad0,STOP,count=10000);assert u.reg_read(UC_X86_REG_EIP)==STOP
   after=bytes(u.mem_read(B+0x3000,0x400));expected=bytearray(before);struct.pack_into('<I',expected,0x7c,2 if health<=0 else 0)
   assert after==expected
   life.append(dict(clock=now,health=health,retired=bool(word(B+0x307c)&2)))
 rows.append(dict(radius=radius,descriptor_delay=delay,deadline=word(B+0x32a4),lifecycle=life))
report=dict(result='PASS',original_sha256=sha,cases=rows,scope='Full466440 descriptor construction with real413700/vector helpers; object allocation and optional texture-name lookup are supplied. Actual4131db..413215 timer setup and full412ad0/4fa3f0/48ab40 execute. No automatic expiry for this terminal terrain-fragment producer; no claim about other generic kind3 producers, freeing or visual acceptance.')
folder=ROOT/'artifacts/geomod-postedit-re';folder.mkdir(exist_ok=True,parents=True)
(folder/'detached-lifetime.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(rows),'factory cases;',sum(len(x['lifecycle']) for x in rows),'full lifecycle cases')
