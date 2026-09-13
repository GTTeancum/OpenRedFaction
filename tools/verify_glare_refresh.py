"""Original414a25 visibility-refresh block, actual4dbc40, versus PC/NXDK."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+16;CAM=B+0x1000;OUT=CAM+32
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');calls={u:0,x:0};case=None
entry=int(re.search(r'\s_rf_glare_refresh_visibility\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,ctx):
 if cpu is u and address in (0x414a73,0x414c69):
  cpu.reg_write(UC_X86_REG_EIP,STOP);return
 if address!=(0x414e00 if cpu is u else CB):return
 sp=cpu.reg_read(UC_X86_REG_ESP);calls[cpu]+=1
 if cpu is u:
  assert r(cpu,sp+4)==B and r(cpu,sp+8)==S+0x30
  value=case[5]
 else:
  assert r(cpu,sp+4)==77 and r(cpu,sp+8)==B and r(cpu,sp+12)==CAM
  cpu.mem_write(r(cpu,sp+16),w(case[5]));value=case[6]
 cpu.reg_write(UC_X86_REG_EAX,value&0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
cases=[(*c,0) for c in itertools.product((0,1,2,0xffffffff),range(4),(-2,-1,0,1),(0,37),(0,1,128),(0,1,258))]
original_count=len(cases);cases += [(h,0,state,37,128,1,-3) for h in (0,1) for state in (-1,0)]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--glare-refresh'],input=b''.join(w(*c) for c in cases));assert len(actual)==20*len(cases)
for n,case in enumerate(cases):
 handle,frame,state,face,old,result,error=case;calls[u]=calls[x]=0
 want_face=0 if state>=0 else face;queried=not((handle^frame)&1)
 want_byte=(result&255) if queried and not error else old
 want_status=error if queried else 0;want_visible=0x12345678 if want_status else int(want_byte!=0)
 if n<original_count:
  u.mem_write(B,bytes(768));u.mem_write(B+0x2c,w(handle));u.mem_write(B+0x298,w(face));u.mem_write(B+0x28d,bytes([old]))
  u.mem_write(0x5a3a34,w(state));u.mem_write(0x175460c,w(frame));u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,S)
  u.emu_start(0x414a25,STOP,count=10000)
  assert u.reg_read(UC_X86_REG_EIP)==STOP and calls[u]==queried
  assert r(u,B+0x298)==want_face and u.mem_read(B+0x28d,1)[0]==want_byte
 x.mem_write(B,bytes(528));x.mem_write(B+112,w(handle));x.mem_write(B+8,bytes([1,old]));x.mem_write(B+20,w(face));x.mem_write(OUT,w(0x12345678))
 x.mem_write(S,w(STOP,B,CAM,frame,state,CB,77,OUT));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(entry,STOP,count=10000)
 got=w(x.reg_read(UC_X86_REG_EAX),r(x,B+20),x.mem_read(B+9,1)[0],calls[x],r(x,OUT))
 want=w(want_status,want_face,want_byte,queried,want_visible)
 assert got==want and actual[n*20:(n+1)*20]==want,(n,case,got,want)
report={'result':'PASS','original_cases':original_count,'callback_error_cases':len(cases)-original_count,'original_sha256':sha,'scope':'Original414a25..414a73/414c69 including actual4dbc40 signed-global predicate, search result supplied. Exact parity/cache-byte/face behavior versus PC and compiled NXDK. Standard active/non-special branch only; parent gates, attenuation and drawing excluded.'}
(root/'artifacts/analysis/glare-refresh.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
