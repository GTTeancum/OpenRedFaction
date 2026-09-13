"""Original special texel edge traversal and row state vs PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW,UC_X86_REG_EBP
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_texel_coverage\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)

TABLE=B+0x5000;FACES=B+0x6000;NODES=B+0x8000;VIEW=B+0x3000
# Stop at sample-search entry or prefix fallback before either executes.
def stop(u,address,size,user):
 if address in (0x4f3720,0x4f3da9):u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop)
rng=random.Random(0x4f35e9);inputs=[];responses=[];hits_total=0;prefix_skips=0;continued=0
for i in range(2048):
 count=i%5;seen=(i//5)%2;counts=[rng.randrange(9) for j in range(4)]
 lo=[rng.uniform(-1,1),rng.uniform(-1,1)];hi=[v+rng.uniform(.01,1) for v in lo]
 points=[[[rng.uniform(-2,2),rng.uniform(-2,2)] for k in range(8)] for j in range(4)]
 if i%7==0:
  counts[0]=4;points[0][:4]=[lo,[hi[0],lo[1]],hi,[lo[0],hi[1]]]
 data=f(*lo,*hi)+w(seen,count,*counts)+b''.join(f(*p) for face in points for p in face)
 inputs.append(data);o.mem_write(STACK,bytes(1024));o.mem_write(STACK+0x5c,w(count,count,TABLE));o.mem_write(STACK+0x78,w(seen))
 # Read the packed float corners to reproduce precisely the native inputs.
 lx,ly,hx,hy=struct.unpack_from('<4f',data)
 for off,p in [(0xc8,(lx,ly)),(0xd0,(hx,ly)),(0xd8,(lx,hy)),(0xc0,(hx,hy))]:o.mem_write(STACK+off,f(*p))
 for j in range(4):
  face=FACES+j*80;node=NODES+j*256;o.mem_write(TABLE+j*4,w(face));o.mem_write(face+64,w(node if counts[j] else 0))
  for k in range(counts[j]):
   # Cover both null-terminated and circular original lists.
   next_node=node+(k+1)*24 if k+1<counts[j] else (node if i%2 else 0)
   o.mem_write(node+k*24,bytes(12)+data[40+j*64+k*8:48+j*64+k*8]+w(next_node))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_EBP,0);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f35e9,STOP,count=1000000)
 endpoint=o.reg_read(UC_X86_REG_EIP);assert endpoint in (0x4f3720,0x4f3da9)
 hits=o.reg_read(UC_X86_REG_EBP);row=struct.unpack('<I',o.mem_read(STACK+0x78,4))[0]
 assert (endpoint==0x4f3720)==bool(row)
 hits_total+=hits;prefix_skips+=endpoint==0x4f3da9;continued+=bool(row and not hits)
 x.mem_write(B,data);x.mem_write(OUT,w(seen,0xa5a5a5a5))
 for j in range(4):x.mem_write(VIEW+j*8,w(B+40+j*64,counts[j]))
 assert call(x,entry,[VIEW,count,B,B+8,OUT,OUT+4])==0
 expected=w(row,hits);assert bytes(x.mem_read(OUT,8))==expected,(i,hits,row,bytes(x.mem_read(OUT,8)).hex())
 responses.append(w(0)+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-texel-coverage']
assert subprocess.check_output(probe,input=b''.join(inputs))==b''.join(responses)
# API guards preserve row state and hit output; original assumes valid owners.
for args in [[0,1,B,B+8,OUT,OUT+4],[VIEW,0,0,B+8,OUT,OUT+4],[VIEW,0,B,B+8,0,OUT+4]]:
 x.mem_write(OUT,w(7,0xa5a5a5a5));assert call(x,entry,args)!=0;assert bytes(x.mem_read(OUT,8))==w(7,0xa5a5a5a5)
report=dict(result='PASS',original_pc_nxdk_texels=len(inputs),edge_hits=hits_total,prefix_skips=prefix_skips,row_continuations_without_hits=continued,nxdk_guards=3,original_sha256=sha,x87_control_word='0x027f',scope='Original4f35e9..4f371a with actual array helpers, linked edge traversal and508f70; only observation stops at sampling/fallback entries. Flat borrowed polygons include closing edges. Texel coordinate generation, sample interpolation, fallback writes and native lighting resources excluded.')
(root/'artifacts/lightmap-texel-coverage.json').write_text(json.dumps(report,indent=2));print(report)
