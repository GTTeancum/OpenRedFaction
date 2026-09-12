"""Original48ca60 list processor versus PC/NXDK; response/query resources supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;nodes=b+0x1000;objects=b+0x4000;views=b+0x8000;backend=b+0x9300;getview=b+0xd000;callback=b+0xd100;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,65536);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_pairs_process\s+([0-9a-fA-F]+)',mapping)[1],16)
ops={0x48cc10:0,0x48bb00:1,0x49ab00:2,0x49a420:3,0x49afe0:4,0x49b570:5}
trace=[];rows=[]
def hook(m,address,size,native):
 if (native and address not in (getview,callback)) or (not native and address not in ops):return
 sp=m.reg_read(UC_X86_REG_ESP);a=struct.unpack('<5I',m.mem_read(sp,20))
 if native and address==getview:value=views+((a[2]-objects)//768)*28
 else:
  if native:op,first,second=a[2:5]
  else:op=ops[address];first=a[1];second=0 if op<2 else a[2]
  trace.extend((op,first,second));value=0
  if op<2:
   index=(first-nodes)//16;assert 0<=index<8;r=rows[index];value=r[4+op]
 m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,a[0])
u.hook_add(UC_HOOK_CODE,hook,False);x.hook_add(UC_HOOK_CODE,hook,True)
def run(m,address,args):
 m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,stop,count=10000)
 assert m.reg_read(UC_X86_REG_EIP)==stop
rng=random.Random(0x48ca60);commands=[];answers=[];counts=[0]*6
idx=lambda p:(p-nodes)//16 if p else 0xffffffff
for case in range(2048):
 order=list(range(8));rng.shuffle(order);n=case%9;active=order[:n];free=order[n:];links=[0xffffffff]*8
 for chain in (active,free):
  for j,i in enumerate(chain):links[i]=chain[j+1] if j+1<len(chain) else 0xffffffff
 rows=[[links[i],rng.randrange(16),rng.randrange(16),rng.getrandbits(6)]+[rng.choice((0,0,1,2,255,256,257)) for _ in range(2)] for i in range(8)]
 actors=[[rng.choice((0,2,3,4,5,7)),rng.choice((0,0x40000000,0x40000000)),rng.choice((0,0,1)),rng.choice((1,1,3)),0x100+i,rng.choice((0xffffffff,0x100+rng.randrange(16))),rng.choice((0,0,0,0x4000))] for i in range(16)]
 heads=[active[0] if active else 0xffffffff,n if case%4 else 0xffffffff,free[0] if free else 0xffffffff,len(free) if case%4 else 0xffffffff]
 words=heads+sum(rows,[])+sum(actors,[]);assert len(words)==164;commands.append(w(*words))
 h=w(nodes+heads[0]*16 if active else 0,heads[1],nodes+heads[2]*16 if free else 0,heads[3]);u.mem_write(0x73db28,h[:8]);u.mem_write(0x75db30,h[8:]);x.mem_write(b,h)
 payload=b''.join(w(nodes+r[0]*16 if r[0]!=0xffffffff else 0,objects+r[1]*768,objects+r[2]*768,r[3]) for r in rows)
 u.mem_write(nodes,payload);x.mem_write(nodes,payload);x.mem_write(views,w(*sum(actors,[])));x.mem_write(backend,w(getview,callback,0))
 for i,(kind,body,model,mode,handle,parent,object_flags) in enumerate(actors):
  obj=objects+i*768;u.mem_write(obj+0x24,w(kind));u.mem_write(obj+0x2c,w(handle));u.mem_write(obj+0x200,w(parent));u.mem_write(obj+0x7c,w(object_flags));u.mem_write(obj+0x1a8,w(body));u.mem_write(obj+0x80,w(model));u.mem_write(obj+0x858,w(b+0xa000+i*8));u.mem_write(b+0xa004+i*8,w(mode))
 trace=[];run(u,0x48ca60,[]);expected=trace[:];trace=[];run(x,entry,[b,b+8,backend]);assert trace==expected,(case,trace,expected)
 result=bytes(u.mem_read(nodes,128));header=bytes(u.mem_read(0x73db28,8))+bytes(u.mem_read(0x75db30,8))
 assert bytes(x.mem_read(nodes,128))==result and bytes(x.mem_read(b,16))==header,case
 out=words[:];hh=struct.unpack('<4I',header);out[:4]=[idx(hh[0]),hh[1],idx(hh[2]),hh[3]]
 for i in range(8):
  link,first,second,flags=struct.unpack('<4I',result[i*16:i*16+16]);out[4+i*6]=idx(link);out[7+i*6]=flags
  assert (first,second)==(objects+rows[i][1]*768,objects+rows[i][2]*768)
 for i in range(0,len(expected),3):counts[expected[i]]+=1
 answers.append(w(*out,len(expected),*expected))
assert all(counts),counts
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--collision-process'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',cases=2048,operation_counts=counts,original_sha256=sha,scope='Full original48ca60 control flow and real list helpers versus PC/NXDK. Expiration, kind5 query and response functions supplied; actual48bb90/40a110 parent/visibility filter executes. Exact traces, ordered arguments, list heads/links/counts, flags and endpoints. Empty/full/permuted lists, consecutive removals, counter wrap, query low bytes, parent matches and visibility flags. Actual collision response effects and live scene scheduling excluded.')
(root/'artifacts/collision-process.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
