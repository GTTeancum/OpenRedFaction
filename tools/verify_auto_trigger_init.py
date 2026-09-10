"""Prepared original loader/constructor blocks versus shared trigger initialization."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_FPCW,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v])
def machine(path):
 p=pefile.PE(str(path));b=p.get_memory_mapped_image();origin=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(b)+4095)//4096*4096);u.mem_write(origin,b);u.mem_map(0x30000000,65536);return u
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');base=0x30000000;params=base+0x1000;stack=base+0xe000;stop=base+0xf000
read=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
records=[r for l in json.loads((root/'artifacts/triggers.json').read_text())['results'] for r in l['records']]
authored=len(records)
for flags in itertools.product((0,1,2),repeat=5):
 records.append(dict(timing=-0.0019,shape=1,flags=flags,box_flag=1,tail_flag=2))
commands=bytearray();expected=bytearray()
for r in records:
 timing=struct.pack('<f',r['timing']);shape=r['shape'];flags=r['flags'];box=r.get('box_flag',r.get('flag',0));disabled=r['tail_flag'];now=12345;handle=0x12340005
 u.mem_write(base,bytes(0x2000));u.mem_write(stack,bytes(0x200));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.mem_write(stack+0x38,timing);u.emu_start(0x46561a,0x46562d,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x46562d
 cooldown=read(u,stack+0x50)
 for offset,value in zip((0x17,0x15,0x13,0x14,0x16),flags):u.mem_write(stack+offset,bytes([value]))
 u.mem_write(stack+0xb8,bytes([box if shape==1 else 0]));u.mem_write(stack+0x58,w(0));u.emu_start(0x465857,0x4658c0,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x4658c0
 runtime_flags=read(u,stack+0x58)
 u.mem_write(params+0xc,w(shape,cooldown,-1,runtime_flags));u.mem_write(params+0x74,w(-1));u.mem_write(0x5a3ed8,w(now));u.mem_write(base+0x2c,w(handle))
 u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBP,params);u.emu_start(0x4bfa46,0x4bfac0,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4bfac0
 if disabled:
  u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(0x4c0210,stop,count=1000);assert u.reg_read(UC_X86_REG_EIP)==stop
 output=w(*[read(u,base+o) for o in (0x2b0,0x2a0,0x298,0x29c,0x2a8,0x2c)])
 commands.extend(timing+w(shape,*flags,box,disabled,handle,now));expected.extend(w(0)+output)
probe=root/'build/pc/Release/rf_event_probe.exe';got=subprocess.check_output([str(probe),'--auto-init'],input=commands)
assert got==expected, next((i for i,(a,b) in enumerate(zip(got,expected)) if a!=b),None)
entry=int(re.search(r'_rf_auto_trigger_init\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for control in (0x7f,0x27f,0x37f):
 for i in range(len(records)):
  raw=commands[i*44:(i+1)*44];fields=struct.unpack('<11I',raw)
  x.mem_write(params,bytes(660));x.mem_write(params+20,w(fields[1]));x.mem_write(params+540,w(*fields[2:7]));x.mem_write(params+564,w(fields[7],fields[8]));x.mem_write(params+592,bytes(raw[:4]));x.mem_write(base,b'\xa5'*24)
  x.mem_write(stack,w(stop,base,params,fields[9],fields[10]));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,control);x.emu_start(entry,stop,count=10000)
  assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_ESP)==stack+4
  assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,24))==expected[i*28:i*28+28],(i,records[i],bytes(x.mem_read(base,24)).hex(),expected[i*28:i*28+28].hex())
report=dict(result='PASS',authored_records=authored,nonbinary_flag_cases=243,nxdk_precision_modes=3,original_sha256=sha,pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Prepared original v180 timing conversion 46561a..46562d, flag packing 465857..4658c0, constructor bookkeeping 4bfa46..4bfac0 and disable 4c0210. PC/NXDK state match. Full parsing/factory/registry/shape creation excluded.')
(root/'artifacts/auto-trigger-init-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)


