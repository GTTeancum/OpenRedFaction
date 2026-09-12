"""Original4154f0 versus shared PC/NXDK corona/reflection pass dispatch."""
import hashlib,itertools,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);return u
original=machine(exe);x=machine(root/'build/xbox/main.exe');sym=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_glare_render_pass\s+([0-9a-fA-F]+)',sym)[1],16)
B=0x30000000;objects=[B+0x1000+i*0x1000 for i in range(3)];L=B+0x5000;V=L+0x100;BE=V+0x100;S=B+0xe000;STOP=B+0xf000;callbacks=[STOP+0x100+i*16 for i in range(3)];orig_callbacks=[0x431950,0x414860,0x4155a0]
traces={original:[],x:[]};mode=0

def hook(cpu,address,length,context):
 shared=cpu is x;ops=callbacks if shared else orig_callbacks
 if address not in ops:return
 op=ops.index(address);sp=cpu.reg_read(UC_X86_REG_ESP);r=lambda a:struct.unpack('<I',cpu.mem_read(a,4))[0];arg=lambda i:r(sp+4+4*i);shift=1 if shared else 0
 if shared:assert arg(0)==123
 event=(0,arg(shift),0) if op==0 else (op,objects.index(arg(shift)),arg(shift+1) if op==1 else 0)
 traces[cpu].append(event);status=0
 if shared and ((mode==1 and event==(0,1,0)) or (mode==2 and op==1) or (mode==3 and op==2) or (mode==4 and event==(0,0,0))):status=0xffffffff
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_EIP,r(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for cpu,addresses in ((original,orig_callbacks),(x,callbacks)):
 for a in addresses:cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(cpu,function,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cpu.emu_start(function,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP
 return cpu.reg_read(UC_X86_REG_EAX)
def canonical(cpu):
 return b''.join(bytes(cpu.mem_read(a+(120 if cpu is x else 0x7c),4))+bytes(cpu.mem_read(a+(48 if cpu is x else 0x2b4),4))+bytes(cpu.mem_read(a+(16 if cpu is x else 0x294),24)) for a in objects)
rng=random.Random(4154);cases=0

def check(count,current,reflections,views,rows,error=0,oracle=True):
 global mode,cases
 mode=error;traces[x]=[];traces[original]=[];before=[]
 for i,a in enumerate(objects):
  raw=bytearray(b'\xa5'*748);raw[0x7c:0x80]=w(rows[i][0]);raw[0x2b4:0x2b8]=w(rows[i][1]);raw[0x294:0x2ac]=w(*rows[i][2:])
  raw[0x2b8:0x2bc]=w(objects[i+1] if i<2 else 0x5c9ba8);original.mem_write(a,bytes(raw));before.append(raw)
  raw=bytearray(528);raw[120:124]=w(rows[i][0]);raw[48:52]=w(rows[i][1]);raw[16:40]=w(*rows[i][2:]);raw[52:60]=w(objects[i+1]+52 if i<2 else L,objects[i-1]+52 if i else L);x.mem_write(a,bytes(raw))
 x.mem_write(L,w(objects[0]+52,objects[-1]+52,3,3));x.mem_write(V,w(*views));x.mem_write(BE,w(*callbacks,123))
 original.mem_write(0x7c7634,w(count));original.mem_write(0x7c763c,w(current));original.mem_write(0x7c75e4,w(*views));original.mem_write(0x5c9e60,w(objects[0]))
 status=run(x,entry,(L,V,count,current,reflections,BE));actual=w(status,len(traces[x]))+b''.join(w(*e) for e in traces[x])+canonical(x)
 pc=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--glare-render-pass'],input=w(count,current,reflections,error,*views)+b''.join(w(*row) for row in rows));assert pc==actual,'PC/NXDK'
 if oracle:
  run(original,0x4154f0,(reflections,));assert status==0 and traces[x]==traces[original] and canonical(x)==canonical(original)
  for a,raw in zip(objects,before):
   # Only marker and the two current-view sample words may change.
   raw[0x2b4:0x2b8]=original.mem_read(a+0x2b4,4);raw[0x294:0x2ac]=original.mem_read(a+0x294,24)
   assert bytes(original.mem_read(a,748))==raw
 else:
  if error and current in views[:count]:
   assert status==0xffffffff
   if error!=1:assert traces[x][-1]==(0,0,0)
  if count>2:assert status==0xfffffffc and not traces[x]
 cases+=1
for n in range(192):
 count=n%3;views=(11,11 if n&1 else 22);current=(11,22,33)[(n//3)%3];reflections=(0,1,255,256,257)[n%5]
 rows=[(rng.choice((0,1,2,0xffffffff)),rng.choice((0,2,0x80000000,0x80000002,0xffffffff)),*[rng.getrandbits(32) for _ in range(6)]) for i in range(3)]
 check(count,current,reflections,views,rows)
rows=[(0,0x80000002,*range(1,7)),(1,0xffffffff,*range(7,13)),(0,2,*range(13,19))]
for error in range(1,5):check(2,11,1,(11,22),rows,error,False)
check(3,11,1,(11,22),rows,0,False)
report=dict(result='PASS',cases=cases,original_sha256=digest,scope='Full original4154f0 with enable/corona/reflection routines supplied versus PC and actual compiled NXDK pass. Ordered calls and state footprints across hidden flags, first view match, duplicate/missing views, reflection low-byte semantics and marker/sample consumption. Four port callback-error cases plus two-view bound. Geometry, visibility-marker production and native rendering remain separate.')
(root/'artifacts/glare-render-pass.json').write_text(json.dumps(report,indent=2));print(report)
