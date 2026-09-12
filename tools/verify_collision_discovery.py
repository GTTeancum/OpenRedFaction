"""Original48c9a0 traversal versus shared PC/NXDK; resource callbacks supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;actor=b+256;sentinel=0x73d880;stack=b+0xe000;stop=b+0xf000;state=b+0x9000;callback=b+0xd000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_pairs_discover\s+([0-9a-fA-F]+)',mapping)[1],16)
trace=[];words=[];reply=0
def hook(m,address,size,native):
 if native:
  if address!=callback:return
 else:
  if address not in (0x48bbe0,0x48bd80):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<5I',m.mem_read(sp,20))
 if native:op,first,second=a[2:5]
 else:op=0 if address==0x48bbe0 else 1;first=a[1];second=0 if op==0 else a[2]
 value=reply
 if op==2:
  assert second==0;value=struct.unpack('<I',m.mem_read(first+16,4))[0]
 else:
  trace.extend((op,first,second))
  if op==0:m.mem_write(state+12 if native else 0x73d890,w(words[5]))
  if op==1 and second==words[6]:m.mem_write(second+16,w(words[7]))
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook,False);x.hook_add(UC_HOOK_CODE,hook,True)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=10000)
 assert m.reg_read(UC_X86_REG_EIP)==stop
rng=random.Random(0x48c9a0);commands=[];answers=[];prepares=visits=0
for case in range(1024):
 order=list(range(32));rng.shuffle(order);order=order[:case%33];links=[sentinel]*32
 for j,i in enumerate(order):links[i]=b+order[j+1]*256 if j+1<len(order) else sentinel
 head=b+order[0]*256 if order else sentinel;alternate=b+order[len(order)//2]*256 if order else sentinel
 mutation=b+order[0]*256 if order and case%3==0 else 0
 kind=(0,1,2,2,4,7)[case%6];flags=(0,0x20,0x100,0x120)[case%4];reply=(0,1,0xffffffff)[case%3]
 words=[actor,kind,flags,head,sentinel,alternate,mutation,sentinel]+links
 commands.append(w(*words));u.mem_write(actor+0x24,w(kind));u.mem_write(actor+0x294,w(b+0x6800));u.mem_write(b+0x6a68,w(flags));u.mem_write(0x73d890,w(head));x.mem_write(state,w(*words[:5]))
 for i in range(32):u.mem_write(b+i*256+16,w(links[i]));x.mem_write(b+i*256+16,w(links[i]))
 trace=[];run(u,0x48c9a0,[actor]);expected=trace[:];trace=[];run(x,entry,[state,callback,0]);assert trace==expected,(case,trace,expected)
 for i in range(32):assert bytes(u.mem_read(b+i*256+16,4))==bytes(x.mem_read(b+i*256+16,4))
 assert bytes(u.mem_read(0x73d890,4))==bytes(x.mem_read(state+12,4))
 prepares+=sum(expected[i]==0 for i in range(0,len(expected),3));visits+=sum(expected[i]==1 for i in range(0,len(expected),3))
 answers.append(w(len(expected),*expected))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--collision-discovery'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',cases=1024,prepares=prepares,pair_attempts=visits,original_sha256=sha,scope='Full original48c9a0 traversal, prepare48bbe0 and create48bd80 callbacks supplied. Exact PC/NXDK callback ordering, head refresh and next-after-create mutation behavior. Empty/full/permuted lists, self candidates and ignored failure returns. Actual projectile preparation, classification, live list ownership and caller scheduling excluded.')
(root/'artifacts/collision-discovery.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
