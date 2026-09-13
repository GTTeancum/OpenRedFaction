"""Full original414860 corona orchestration versus composed PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
f=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
B=0x30000000;S=B+0xe000;STOP=B+0xf000;CB=STOP+16;FRAME=B+0x1000;DEF=B+0x2000;BE=B+0x3000;VIEW=B+0x6000;ROOM=B+0x7000;PARENT=B+0x8000;SPECIAL=B+0x9000
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(B,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_glare_corona_render\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
original=[0x40a0e0,0x414e00,0x5031f0,0x416450,0x50cf80,0x50d060,0x515b40,0x515bd0];callbacks=[CB+i*16 for i in range(8)];traces={u:[],x:[]};snapshots=[];case=None
identity=w(*map(f,[1,0,0,0,1,0,0,0,1]));glare_basis=w(*map(f,[-1,0,0,0,1,0,0,0,-1]));codes=[6,7,8,9,2,3,4,5]
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+i*4)
 if cpu is u and address in (0x40d760,0x40d780):
  cpu.mem_write(arg(0),bytes(12) if address==0x40d760 else identity)
  cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4);return
 addresses=original if cpu is u else callbacks
 if address not in addresses:return
 op=addresses.index(address);shift=0 if cpu is u else 1;result=0
 if cpu is x:assert arg(0)==77
 if op==0:
  assert arg(shift)==32;payload=[32]
  if cpu is u:result=PARENT if case[8] else 0
  else:cpu.mem_write(arg(2),w(case[8]!=2))
 elif op==1:
  assert arg(shift)==B;payload=[case[0]]
  if cpu is u:result=case[9]
  else:cpu.mem_write(arg(3),w(case[9]))
 elif op==2:
  payload=[1]
  if cpu is u:assert arg(0)==77;result=case[10]==2
  else:assert arg(1)==B;cpu.mem_write(arg(3),w(case[10]==1))
 elif op==3:payload=[arg(1+i) for i in range(4)]
 elif op==4:payload=[arg(shift+i) for i in range(4)]
 elif op==5:payload=[arg(shift+i) for i in range(2)]
 elif op==6:
  assert arg(shift)==B+(0x3c if cpu is u else 152);payload=[arg(shift+i) for i in (1,2,3)]
 else:
  assert arg(shift)==B+(0x2d4 if cpu is u else 80) and arg(shift+1)==B+(0x2e0 if cpu is u else 92);payload=[0,arg(shift+2),arg(shift+3)]
 traces[cpu].append([codes[op],*payload,*([0]*(7-len(payload)))])
 if cpu is u:snapshots.append(bytes(cpu.mem_read(B,768)))
 elif case[17]==len(traces[cpu]):result=-1
 cpu.reg_write(UC_X86_REG_EAX,result&0xffffffff);cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
u.hook_add(UC_HOOK_CODE,hook);x.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(4148600);cases=[]
for n in range(256):cases.append([n,rng.randrange(4),rng.randrange(2),rng.randrange(2),rng.choice((0,37)),rng.choice((-1,0)),rng.choice((-1,77,77)),n%2,rng.randrange(4),rng.randrange(2),rng.randrange(3),rng.randrange(2),f(rng.choice((.5,1,2))),f(rng.choice((3,8,30))),f(rng.choice((0,1,20))),f(rng.choice((0,.01,.2,1))),f(rng.choice((0,.01,.3,2))),0])
for special in (0,1,2):cases.append([4,4,1,0,37,0,77,1,3,1,special,0,f(1),f(3),f(0),f(.2),f(.3),0])
base=len(cases)
for oriented in (0,1):
 for error in range(1,7):cases.append([4,4,1,0,37,0,77,1,3,1,0,oriented,f(1),f(3),f(0),f(.2),f(.3),error])
actual=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--corona-frame'],input=b''.join(w(*c) for c in cases));assert len(actual)==292*len(cases)
for n,case in enumerate(cases):
 traces[u]=[];traces[x]=[];snapshots=[];samples=w(case[15],case[15],case[16],case[16])
 u.mem_write(B,bytes(768));u.mem_write(B+0x2c,w(case[0],32));u.mem_write(B+0x3c,w(case[14],0,case[13]));u.mem_write(B+0x48,glare_basis);u.mem_write(B+0x78,w(f(1)));u.mem_write(B+0x28c,bytes([case[2],case[3]]));u.mem_write(B+0x298,w(case[4]));u.mem_write(B+0x29c,samples);u.mem_write(B+0x2ac,w(DEF));u.mem_write(B+0x2cc,w(SPECIAL if case[10] else 0));u.mem_write(B+0x2d0,bytes([case[11]]))
 u.mem_write(DEF,bytes(128));u.mem_write(DEF+8,bytes([255,80,40,0]));u.mem_write(DEF+12,w(0 if case[6]==-1 else case[6],f(45),case[12],f(1),f(1),0));u.mem_write(PARENT,w(ROOM if case[8]>=2 else 0));u.mem_write(ROOM+0x161,bytes([case[8]==3]));u.mem_write(SPECIAL+0x34,w(77));u.mem_write(SPECIAL+0xff4,identity);u.mem_write(SPECIAL+0x1018,bytes(12));u.mem_write(0x7c763c,w(VIEW));u.mem_write(VIEW+0xc4,w(1));u.mem_write(0x5a3a34,w(case[5]));u.mem_write(0x175460c,w(case[1]))
 u.mem_write(S,w(STOP,B,case[7]));u.reg_write(UC_X86_REG_ESP,S);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(0x414860,STOP,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==STOP
 failed=case[17]>0 and case[17]<=len(traces[u]);trace=traces[u][:case[17]] if failed else traces[u];raw=snapshots[case[17]-1] if failed else bytes(u.mem_read(B,768))
 want=w(-1 if failed else 0)+raw[0x78:0x7c]+raw[0x29c:0x2ac]+raw[0x298:0x29c]+w(raw[0x28d],len(trace))+b''.join(w(*row) for row in trace)+bytes(32*(8-len(trace)))
 x.mem_write(B,bytes(528));x.mem_write(B+112,w(case[0]));x.mem_write(B+128,w(32));x.mem_write(B+144,w(f(1)));x.mem_write(B+152,w(case[14],0,case[13]));x.mem_write(B+164,glare_basis);x.mem_write(B+8,bytes([case[2],case[3]]));x.mem_write(B+20,w(case[4]));x.mem_write(B+24,samples);x.mem_write(B+72,w(1 if case[10] else 0));x.mem_write(B+76,bytes([case[11]]))
 x.mem_write(DEF,bytes(320));x.mem_write(DEF+256,w(255,80,40,f(45),case[12],f(1),f(1),0));x.mem_write(FRAME,bytes(12)+identity+w(f(90),f(.5),f(.5),case[1],case[7],case[5],case[6]));x.mem_write(BE,w(*callbacks[:4],77,*callbacks[4:],77));x.mem_write(S,w(STOP,B,DEF,FRAME,BE));x.reg_write(UC_X86_REG_ESP,S);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,STOP,count=1000000)
 got=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(B+144,4))+bytes(x.mem_read(B+24,16))+bytes(x.mem_read(B+20,4))+w(x.mem_read(B+9,1)[0],len(traces[x]))+b''.join(w(*row) for row in traces[x])+bytes(32*(8-len(traces[x])))
 assert got==want and actual[n*292:(n+1)*292]==want,(n,case,got.hex(),want.hex(),actual[n*292:(n+1)*292].hex())
report={'result':'PASS','original_cases':base,'callback_error_cases':len(cases)-base,'original_sha256':sha,'scope':'Full414860 and actual math/packed-mode/room-word/special query construction execute. Only parent lookup, camera access, occlusion/model result, flash and graphics supplied. Exact owner state and ordered callbacks versus composed PC/NXDK; special model geometry remains callback-supplied. No native rendering claim.'}
(root/'artifacts/analysis/corona-frame.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
