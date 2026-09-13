"""Original414cef corona tail and real arithmetic versus PC/NXDK callbacks."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+16;TAIL=B+0x1000;BE=TAIL+64;DEF=TAIL+128
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_corona_submit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
original=[0x50cf80,0x50d060,0x515b40,0x515bd0];callbacks=[CB+i*16 for i in range(4)];traces={u:[],x:[]};case=None
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 if cpu is u and address in (0x414dcd,0x414df0):cpu.reg_write(UC_X86_REG_EIP,STOP);return
 addresses=original if cpu is u else callbacks
 if address not in addresses:return
 op=addresses.index(address);sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+i*4);shift=0 if cpu is u else 1
 if cpu is x:assert arg(0)==77
 if op==0:payload=[arg(shift+i) for i in range(4)]
 elif op==1:payload=[arg(shift+i) for i in range(2)]
 elif op==2:
  assert arg(shift)==B+(0x3c if cpu is u else 152)
  payload=[arg(shift+i) for i in (1,2,3)]
 else:
  assert arg(shift)==B+(0x2d4 if cpu is u else 80) and arg(shift+1)==B+(0x2e0 if cpu is u else 92)
  payload=[0,arg(shift+2),arg(shift+3)]
 traces[cpu].append([op+2,*payload,*([0]*(7-len(payload)))])
 result=-1 if cpu is x and case[10]==len(traces[cpu]) else 0
 cpu.reg_write(UC_X86_REG_EAX,result&0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
samples=w(f(.1),f(.2),f(.3),f(.4));rng=random.Random(414)
cases=[]
for view,draw,oriented,side,intensity,size in itertools.product((0,1),(0,1),(0,255),(-1,0,1),(0,.5,1),(.02,2)):
 cases.append([f(intensity),f(size),f(.3),f(25),f(side),f(1),97,view,draw,oriented,0])
for n in range(256):cases.append([f(rng.random()),f(rng.uniform(0,10)),f(rng.uniform(-2,2)),f(rng.uniform(.1,100)),f(rng.uniform(-1,1)),f(rng.uniform(0,10)),rng.randrange(100),n%2,1,n%3,0])
base=len(cases)
for oriented,error in itertools.product((0,1),range(1,4)):cases.append([f(.7),f(3),f(.2),f(50),f(-1),f(1),97,1,1,oriented,error])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--corona-tail'],input=b''.join(w(*c) for c in cases));assert len(actual)==156*len(cases)
for n,case in enumerate(cases):
 intensity,size,angular,distance,side,radius,bitmap,view,draw,oriented,error=case;traces[u]=[];traces[x]=[]
 u.mem_write(B,bytes(768));u.mem_write(B+0x78,w(radius));u.mem_write(B+0x29c,samples);u.mem_write(B+0x2d0,bytes([oriented]));u.mem_write(DEF+12,w(bitmap))
 u.mem_write(S,bytes(512));u.mem_write(S+0x13,bytes([draw]));u.mem_write(S+0x14,w(intensity,angular,0,size,distance,0,B+0x3c));u.mem_write(S+0x3c,w(f(1),0,0));u.mem_write(S+0x60,w(side,0,0))
 u.reg_write(UC_X86_REG_ESI,B);u.reg_write(UC_X86_REG_EDI,DEF);u.reg_write(UC_X86_REG_EBP,view);u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x414cef,STOP,count=100000)
 assert u.reg_read(UC_X86_REG_EIP)==STOP
 want_trace=traces[u][:error] if error else traces[u];status=-1 if error else 0
 want=w(status,r(u,B+0x78))+bytes(u.mem_read(B+0x29c,16))+w(len(want_trace))+b''.join(w(*row) for row in want_trace)+bytes(32*(4-len(want_trace)))
 x.mem_write(B,bytes(528));x.mem_write(B+144,w(radius));x.mem_write(B+24,samples);x.mem_write(B+76,bytes([oriented]));x.mem_write(TAIL,w(intensity,size,angular,side,bitmap,view,draw));x.mem_write(BE,w(*callbacks,77));x.mem_write(S,w(STOP,B,TAIL,BE))
 x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=100000)
 got=w(x.reg_read(UC_X86_REG_EAX),r(x,B+144))+bytes(x.mem_read(B+24,16))+w(len(traces[x]))+b''.join(w(*row) for row in traces[x])+bytes(32*(4-len(traces[x])))
 assert got==want and actual[n*156:(n+1)*156]==want,(n,case,got.hex(),want.hex())
report={'result':'PASS','original_cases':base,'callback_error_cases':len(cases)-base,'original_sha256':sha,'scope':'Original414cef..414dfa actual411e00 packed-mode constructor and ftol/max/dot arithmetic with graphics callbacks supplied versus PC/NXDK. Exact alpha, sample/radius publication, angle sign, color/texture sequence and packed geometry mode and billboard/oriented arguments. Upstream side-dot supplied to shared tail; original computes equivalent axial dot. No native drawing claim.'}
(root/'artifacts/analysis/corona-tail.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
