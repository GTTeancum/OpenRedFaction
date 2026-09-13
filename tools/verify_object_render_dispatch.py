"""Original488b20 dispatch versus PC and compiled NXDK; graphics services supplied."""
import hashlib,itertools,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+0x100;BE=B+0x1000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_object_render_dispatch\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
renders=[0x421850,0x458f80,0x4c72c0,0x412d90,0x4103d0,0x4c05a0,0x4bd7e0,0x417080,0x46a9c0,0x46b2b0,0x4141a0]
original={0x50cf80:1,0x502b00:3,0x503360:4,**{a:5+i for i,a in enumerate(renders)}}
trace={u:[],x:[]};case=None
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,ctx):
 if cpu is u:
  if address not in original:return
  code=original[address]
 else:
  callbacks={CB:1,CB+16:3,CB+32:4,CB+48:5+case[0]}
  if address not in callbacks:return
  code=callbacks[address]
 trace[cpu].append(code);sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+i*4)
 if cpu is u:
  if code>=5:
   assert arg(0)==(B-4 if case[0]==6 else B)
   assert r(cpu,B+0x7c)==case[1]
  value=case[3] if code==3 else 0
 else:
  assert arg(0)==77
  if code==3:assert arg(1)==case[2];cpu.mem_write(arg(2),w(case[3]))
  if code==4:assert arg(1)==case[2]
  if code>=5:assert arg(1)==case[0]
  value=0xffffffff if case[5]==len(trace[cpu]) else 0
 cpu.reg_write(UC_X86_REG_EAX,value);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
def run(cpu,address,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.emu_start(address,STOP,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;return cpu.reg_read(UC_X86_REG_EAX)
cases=[(*c,0,0) for c in itertools.product(range(11),(0,2,16,0x100,0x4000,0x4010,0x4002,0xffffffff),(0,1),(1,3))]
base=len(cases);cases += [(0,0,1,3,0,i) for i in range(1,5)]
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--object-render-dispatch'],input=b''.join(w(*c) for c in cases));assert len(actual)==44*len(cases)
rendered=skipped=0
for n,case in enumerate(cases):
 typ,flags,model,kind,reserved,error=case;trace[u]=[];trace[x]=[]
 if n<base:
  u.mem_write(B,bytes(768));u.mem_write(B+0x24,w(typ));u.mem_write(B+0x7c,w(flags,model));run(u,0x488b20,(B,))
  want_flags=r(u,B+0x7c);want_trace=trace[u];status=0
  skip=bool(flags&2 or model and flags&0x4000);assert want_flags==(flags if skip else flags|16)
  assert [v for v in want_trace if v>=5]==([] if skip else [5+typ]);rendered+=not skip;skipped+=skip
 else:want_flags=flags;want_trace=[1,3,4,5][:error];status=0xffffffff
 x.mem_write(B,w(flags));x.mem_write(BE,w(*range(CB,CB+64,16),77))
 got=run(x,entry,(B,typ,model,BE));assert got==status and r(x,B)==want_flags and trace[x]==want_trace,(n,case,trace[x],want_trace)
 want=w(status,want_flags,len(want_trace),*want_trace,*([0]*(8-len(want_trace))))
 assert actual[n*44:(n+1)*44]==want,(n,case,'PC')
report={'result':'PASS','original_cases':base,'callback_error_cases':len(cases)-base,'rendered':rendered,'skipped':skipped,'original_sha256':sha,'scope':'Full original488b20 dispatcher, actual40a110 predicate executes; graphics/model/family callbacks supplied; exact order and post-render marker versus PC/NXDK. Callback errors are port guards. No live renderer, marker clearing or room scheduling claim.'}
(root/'artifacts/object-render-dispatch.json').write_text(json.dumps(report,indent=2));print(report)
