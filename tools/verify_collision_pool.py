"""Original fixed pool initialization and pair allocation; gate classification supplied."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
b=0x30000000;nodes=b+0x1000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*(n&0xffffffff for n in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
 m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(base,(len(im)+4095)//4096*4096);m.mem_write(base,im);m.mem_map(b,0x40000);return m
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
symbol=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def run(m,address,args,end=stop):
 m.mem_write(stack,w(end,*args));m.reg_write(UC_X86_REG_ESP,stack);m.emu_start(address,end,count=300000)
 assert m.reg_read(UC_X86_REG_EIP)==end;return m.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(0x48bd80)
# Full8192 record startup: stop only before CRT atexit registration.
payload=rng.randbytes(8192*16);u.mem_write(0x73db30,payload);u.mem_write(0x75db30,w(0,0));x.mem_write(b+0x10000,payload);x.mem_write(b,w(0,0))
run(u,0x48c950,[],0x48c96c);run(x,symbol('rf_collision_pairs_seed'),[b,b+0x10000,8192])
assert bytes(u.mem_read(0x75db30,8))==w(0x75db20,8192)
assert bytes(x.mem_read(b,8))==w(b+0x10000+8191*16,8192)
for i in range(8192):
 a=bytes(u.mem_read(0x73db30+i*16,16));c=bytes(x.mem_read(b+0x10000+i*16,16))
 assert a==w(0x73db30+(i-1)*16 if i else 0)+payload[i*16+4:i*16+16]
 assert c==w(b+0x10000+(i-1)*16 if i else 0)+a[4:]
reply=flags=calls=0
first=second=0
def gate(m,address,size,which):
 global calls
 if address!=(0x48be00 if which=='original' else b+0xd000):return
 sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',m.mem_read(sp,20))
 left,right,out=args[1:4] if which=='original' else args[2:5]
 assert (left,right)==(first,second) and bytes(m.mem_read(out,4))==w(0)
 m.mem_write(out,w(flags));calls+=1;m.reg_write(UC_X86_REG_EAX,reply);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,args[0])
u.hook_add(UC_HOOK_CODE,gate,'original');x.hook_add(UC_HOOK_CODE,gate,'native')
commands=[];answers=[];created=0
idx=lambda ptr:(ptr-nodes)//16 if ptr else 0xffffffff
for case in range(2048):
 order=list(range(32));rng.shuffle(order);n=case%33;f=rng.randrange(33-n);active=order[:n];free=order[n:n+f]
 body=bytearray(rng.randbytes(512))
 for chain in (active,free):
  for k,i in enumerate(chain):body[i*16:i*16+4]=w(nodes+chain[k+1]*16 if k+1<len(chain) else 0)
 for i in order[n+f:]:body[i*16:i*16+4]=w(0)
 ac=n if case%4 else 0xffffffff;fc=f if case%4 else 0
 heads=w(nodes+active[0]*16 if active else 0,ac,nodes+free[0]*16 if free else 0,fc)
 reply=(0,1,2,255,256,257,0xffffff00,0xffffffff)[case%8];flags=rng.getrandbits(32);first=rng.getrandbits(32);second=rng.getrandbits(32)
 def wire(header,data,result,counter):
  h=struct.unpack('<4I',header);out=[idx(h[0]),h[1],idx(h[2]),h[3],first,second,reply,flags,counter,result]
  for i in range(32):
   a,c,d,e=struct.unpack('<4I',data[i*16:i*16+16]);out.extend((idx(a),c,d,e))
  return w(*out)
 commands.append(wire(heads,body,0,0));u.mem_write(nodes,bytes(body));x.mem_write(nodes,bytes(body));u.mem_write(0x73db28,heads[:8]);u.mem_write(0x75db30,heads[8:]);x.mem_write(b,heads)
 calls=0;result=run(u,0x48bd80,[first,second]);assert calls==1
 calls=0;actual=run(x,symbol('rf_collision_pair_create'),[b,b+8,first,second,b+0xd000,0]);assert calls==1 and actual==result
 after=bytes(u.mem_read(nodes,512));header=bytes(u.mem_read(0x73db28,8))+bytes(u.mem_read(0x75db30,8))
 assert bytes(x.mem_read(nodes,512))==after and bytes(x.mem_read(b,16))==header
 assert result==int((reply&255)!=1 and bool(free));created+=result
 answers.append(wire(header,after,result,1))
assert subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--collision-create'],input=b''.join(commands))==b''.join(answers)
report=dict(result='PASS',seed_records=8192,seed_bytes=131072,creation_cases=2048,created=created,original_sha256=sha,scope='Original48c950 through before CRT atexit versus NXDK seed, full record payload preservation. Original48bd80 allocation with real list helpers versus PC/NXDK, gate48be00 supplied including noncanonical low bytes. Capacity exhaustion and counter wrap. Classification, broad-phase discovery, live pool ownership and XEMU execution excluded.')
(root/'artifacts/collision-pool.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
