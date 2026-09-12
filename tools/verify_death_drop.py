"""Configured SP death-item drop against unchanged original placement arithmetic."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v));f=lambda *v:struct.pack('<'+'f'*len(v),*v)
b=0x30000000;stack=b+0x1d000;stop=b+0x1e000;item=b+0x4000;name=b+0x6000
binary=root/'Installed_Game/RF.exe';digest=hashlib.sha256(binary.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;m=Uc(UC_ARCH_X86,UC_MODE_32)
 m.mem_map(ib,(len(im)+4095)//4096*4096);m.mem_write(ib,im);m.mem_map(b,0x20000);return m
u=machine(binary);x=machine(root/'build/xbox/main.exe')
entry=int(re.search(r'\s_rf_entity_death_drop\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
get=lambda m,a:struct.unpack('<I',m.mem_read(a,4))[0]
def ret(m,value=0,pop=0):
 sp=m.reg_read(UC_X86_REG_ESP);target=get(m,sp);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_EIP,target)
queries=bytes(24);point=bytes(12);calls=[0,0,0];events=[];hit=b'';present=0;size=0;index=0;owner=0
identity=f(1,0,0,0,1,0,0,0,1)
def original_hook(m,a,n,data):
 global queries,point
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4df1c0:
  q=get(m,sp+4);out=get(m,sp+8);assert get(m,sp+12)==1
  assert bytes(m.mem_read(q+0x4c,8))==f(.15)+w(0x2000) and get(m,out+4)==0x7f7fffff
  assert get(m,q)==0 and bytes(m.mem_read(q+4,12))==bytes(12) and bytes(m.mem_read(q+0x10,36))==identity
  queries=bytes(m.mem_read(q+0x34,24));calls[0]+=1;events.append('query')
  m.mem_write(out,hit[:4]);m.mem_write(out+8,hit[4:]);ret(m,0,12)
 elif a==0x459100:
  assert get(m,sp+4)==index and get(m,sp+12)==37 and get(m,sp+16)==owner
  assert get(m,get(m,sp+8))==0
  assert bytes(m.mem_read(get(m,sp+24),36))==identity
  assert get(m,sp+28)==0xffffffff and get(m,sp+32)==1 and get(m,sp+36)==0
  point=bytes(m.mem_read(get(m,sp+20),12));calls[1]+=1;events.append('create');ret(m,item if present else 0)
 elif a==0x503310:
  assert get(m,sp+4)==0x12345678;calls[2]+=1;events.append('bounds')
  m.mem_write(get(m,sp+8),f(-1,-2,-3));m.mem_write(get(m,sp+12),f(size,9,17));ret(m)
 elif a in (0x500290,0x5001d0):
  text=bytes(m.mem_read(get(m,sp+8),32)).split(b'\0')[0].decode();events.append(('neq:' if a==0x500290 else 'eq:')+text)
def native_hook(m,a,n,data):
 global queries,point
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==b+0x3100:
  queries=bytes(m.mem_read(get(m,sp+8),12))+bytes(m.mem_read(get(m,sp+12),12));calls[0]+=1
  m.mem_write(get(m,sp+16),hit);ret(m,0xffffffff if present==2 else 0)
 elif a==b+0x3200:
  assert get(m,sp+8)==index and get(m,sp+12)==owner
  point=bytes(m.mem_read(get(m,sp+16),12));calls[1]+=1;ret(m,item if present else 0)
 elif a==b+0x3300:
  assert get(m,sp+8)==0x12345678;calls[2]+=1;m.mem_write(get(m,sp+12),f(size));ret(m,0xffffffff if present==3 else 0)
u.hook_add(UC_HOOK_CODE,original_hook);x.hook_add(UC_HOOK_CODE,native_hook)
rng=random.Random(0x4200c6);commands=[];expected=[];counts=dict(disabled=0,miss=0,empty_inventory=0,allocation_failure=0,ordinary=0,special=0)
for case in range(1024):
 index=-1 if case%11==0 else case%16;owner=rng.getrandbits(32);position=f(*[rng.uniform(-100,100) for _ in range(3)]);extent=f(rng.uniform(.1,5))
 owned=bytearray(64)
 if case%7:owned[case%64]=rng.choice([1,2,128,255])
 source=w(index,owner)+position+extent+owned
 count=rng.choice([-1,0,1,1,2]);normal=rng.choice([(0,1,0),(1,0,0),(0,0,1),(.25,.75,.5),(-.5,.5,.25)])
 hit=w(count)+f(*[rng.uniform(-100,100) for _ in range(3)])+f(*normal)
 flags=rng.getrandbits(32);item_words=w(flags,0x12345678)+f(*[rng.uniform(-100,100) for _ in range(6)])
 size=rng.uniform(-2,2);present=case%5!=0;classname=rng.choice([b'medical kit',b'MEDICAL KIT',b'medical_kit',b'riot_stick_battery',b'RIOT_STICK_BATTERY',b'rifle',b'',b'medical_kit_extra'])
 queries=bytes(24);point=bytes(12);calls=[0,0,0];events=[]
 u.mem_write(b,bytes(0x1c000));u.mem_write(stack,bytes(0x300));u.mem_write(b+0x82c,w(index));u.mem_write(b+0x2c,w(owner));u.mem_write(b+0x3c,position);u.mem_write(b+0x48,identity);u.mem_write(b+0x7c4,extent);u.mem_write(b+0x42c,bytes(owned))
 if index>=0:u.mem_write(0x6430cc+index*80,w(37))
 u.mem_write(item+0x2bc,item_words[:4]);u.mem_write(item+0x80,item_words[4:8]);u.mem_write(item+0x3c,item_words[8:20]);u.mem_write(item+0xe4,item_words[20:32]);u.mem_write(item+0x294,w(name));u.mem_write(name,w(len(classname),name+16));u.mem_write(name+16,classname+b'\0');u.mem_write(0x20852f4,w(0))
 body=bytes(u.mem_read(b,0x1500));u.reg_write(UC_X86_REG_ESI,b);u.reg_write(UC_X86_REG_EBP,0xffffffff);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4200c6,0x4204a1,count=20000);assert u.reg_read(UC_X86_REG_EIP)==0x4204a1
 assert bytes(u.mem_read(b,0x1500))==body
 created=get(u,stack+0x10)!=0
 out_item=bytes(u.mem_read(item+0x2bc,4))+bytes(u.mem_read(item+0x80,4))+bytes(u.mem_read(item+0x3c,12))+bytes(u.mem_read(item+0xe4,12))
 original_result=w(0,created)+out_item+queries+point+w(*calls)
 if index==-1:counts['disabled']+=1
 elif count<=0:counts['miss']+=1
 elif not any(owned):counts['empty_inventory']+=1
 elif not present:counts['allocation_failure']+=1
 elif calls[2]:counts['ordinary']+=1
 else:counts['special']+=1
 queries=bytes(24);point=bytes(12);calls=[0,0,0]
 x.mem_write(b,source);x.mem_write(item,item_words+w(name));x.mem_write(name,classname+b'\0')
 x.mem_write(b+0x2000,w(b+0x3100,b+0x3200,b+0x3300,0));x.mem_write(b+0x2100,w(0));x.mem_write(stack,w(stop,b,b+0x2000,b+0x2100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop
 got=w(x.reg_read(UC_X86_REG_EAX),get(x,b+0x2100)!=0)+bytes(x.mem_read(item,32))+queries+point+w(*calls)
 assert got==original_result,(case,struct.unpack('<22I',got),struct.unpack('<22I',original_result),events)
 commands.append(source+hit+item_words+f(size)+w(present)+classname.ljust(32,b'\0'));expected.append(original_result)
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-drop'],input=b''.join(commands))==b''.join(expected)
# Port-only query/bounds failures: preserve a created item for caller cleanup.
for present in (2,3):
 index=1;owner=7;size=.5;hit=w(1)+f(7,8,9,0,1,0)
 source=w(index,owner)+f(0,1,0,1)+bytes([1])+bytes(63)
 item_words=w(0x10,0x12345678)+f(1,2,3,4,5,6);classname=b'rifle'
 queries=bytes(24);point=bytes(12);calls=[0,0,0]
 x.mem_write(b,source);x.mem_write(item,item_words+w(name));x.mem_write(name,classname+b'\0')
 x.mem_write(b+0x2000,w(b+0x3100,b+0x3200,b+0x3300,0));x.mem_write(b+0x2100,w(0));x.mem_write(stack,w(stop,b,b+0x2000,b+0x2100));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=20000);assert x.reg_read(UC_X86_REG_EIP)==stop
 want=w(-1,present==3)+w(0x18 if present==3 else 0x10)+item_words[4:]+f(0,1.5,0,0,-1.5,0)+(f(7,8,9) if present==3 else bytes(12))+w(1,present==3,present==3)
 got=w(x.reg_read(UC_X86_REG_EAX),get(x,b+0x2100)!=0)+bytes(x.mem_read(item,32))+queries+point+w(*calls)
 assert got==want
 payload=source+hit+item_words+f(size)+w(present)+classname.ljust(32,b'\0')
 assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--death-drop'],input=payload)==want
report=dict(result='PASS',cases=len(commands),port_failure_cases=2,branches=counts,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Original4200c6..4204a0 with actual vector arithmetic and string comparisons, supplied4df1c0 hit,459100 creation and503310 bounds. Exact PC/NXDK query, creation point, flags and item positions. Query before inventory scan, identity creation basis, special name offsets, null creation. No item allocator, authored collision integration, weapon-drop42ae10 or corpse publication.')
(root/'artifacts/death-drop.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
