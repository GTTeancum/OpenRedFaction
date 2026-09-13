"""Original special sample search and interpolation vs PC/NXDK."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
B=0x30000000;OUT=B+0x4000;STACK=B+0xe000;STOP=B+0xff00
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase;u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(base,(len(im)+4095)//4096*4096);u.mem_write(base,im);u.mem_map(B,65536);return u
o=machine(exe);x=machine(root/'build/xbox/main.exe');mp=(root/'build/xbox/main.map').read_text();entry=int(re.search(r'\s_rf_lightmap_select_sample\s+([0-9a-fA-F]+)',mp)[1],16)
def call(u,entry,args):
 u.mem_write(STACK,w(STOP,*args));u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPCW,0x27f);u.emu_start(entry,STOP,count=100000);assert u.reg_read(UC_X86_REG_EIP)==STOP;return u.reg_read(UC_X86_REG_EAX)

TABLE=B+0x5000;FACES=B+0x6000;NODES=B+0x8000;NORMALS=B+0x7000;VIEW=B+0x3000
rng=random.Random(0x4f3720);inputs=[];responses=[];modes=[0,0,0];passes={}
def stop(u,address,size,user):
 if address in (0x4f3d29,0x4f39a7):u.emu_stop()
o.hook_add(UC_HOOK_CODE,stop)
for i in range(2048):
 count=i%5;counts=[rng.randrange(9) for j in range(4)];center=[rng.uniform(-2,2),rng.uniform(-2,2)];radius=rng.choice([0,.01,.25,2])
 vertices=[[[rng.uniform(-2,2),rng.uniform(-2,2)]+[rng.uniform(-100,100) for k in range(3)]+[rng.uniform(-1,1) for k in range(3)] for j in range(8)] for face in range(4)]
 if i%8==0:
  count=1;counts[0]=4;center=[0,0];radius=0
  for j,v in enumerate(vertices[0]):v[:2]=[-1 if j<2 else 1, .0001*2**((i//8)%11)+(0 if j%2==0 else 1)]
 if i%8==1:
  count=1;counts[0]=3;radius=1;center=[0,0]
  for j,v in enumerate(vertices[0]):v[:2]=[.5 if j==0 else .1,0]
 data=f(*center,radius)+w(count,*counts)+b''.join(f(*v) for face in vertices for v in face);inputs.append(data)
 o.mem_write(B,data);o.mem_write(STACK,bytes(1024));o.mem_write(STACK+0x5c,w(count,count,TABLE));o.mem_write(STACK+0x48,w(NORMALS))
 o.mem_write(STACK+0x18,data[:4]);o.mem_write(STACK+0x24,data[4:8]);o.mem_write(STACK+0x2c,data[8:12])
 for j in range(4):
  face=FACES+j*80;node=NODES+j*256;o.mem_write(TABLE+j*4,w(face));o.mem_write(face+64,w(node if counts[j] else 0))
  norm=NORMALS+0x100+j*128;o.mem_write(NORMALS+j*12,w(counts[j],counts[j],norm))
  for k in range(counts[j]):
   p=B+32+j*256+k*32;next_node=node+(k+1)*24 if k+1<counts[j] else (node if i%2 else 0)
   o.mem_write(node+k*24,w(p+8)+bytes(8)+bytes(o.mem_read(p,8))+w(next_node));o.mem_write(norm+k*12,bytes(o.mem_read(p+20,12)))
 o.reg_write(UC_X86_REG_ESP,STACK);o.reg_write(UC_X86_REG_FPCW,0x27f);o.emu_start(0x4f3720,STOP,count=1000000)
 endpoint=o.reg_read(UC_X86_REG_EIP);assert endpoint in (0x4f3d29,0x4f39a7)
 kind=0 if endpoint==0x4f39a7 else (1 if o.mem_read(STACK+0x13,1)[0] else 2)
 expected=bytes([165])*24 if not kind else bytes(o.mem_read(STACK+0xf8,12))+bytes(o.mem_read(STACK+0xe0,12))
 iteration=struct.unpack('<I',o.mem_read(STACK+0x4c,4))[0];passes[iteration]=passes.get(iteration,0)+1;modes[kind]+=1
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*28)
 for j in range(4):x.mem_write(VIEW+j*8,w(B+32+j*256,counts[j]))
 assert call(x,entry,[VIEW,count,B,struct.unpack('<I',data[8:12])[0],OUT,OUT+24])==0
 got=bytes(x.mem_read(OUT,24));actual=struct.unpack('<I',x.mem_read(OUT+24,4))[0];assert (actual,got)==(kind,expected),(i,iteration,kind,actual,got.hex(),expected.hex())
 responses.append(w(0,kind)+expected)
probe=[str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-select-sample']
got=subprocess.check_output(probe,input=b''.join(inputs));assert got==b''.join(responses),next((i for i in range(len(inputs)) if got[i*32:i*32+32]!=responses[i]),-1)
guards=[]
for offset,value in [(0,math.nan),(8,-1.0),(8,math.inf)]:
 bad=bytearray(inputs[0]);bad[offset:offset+4]=f(value);guards.append(bytes(bad))
for data in guards:
 x.mem_write(B,data);x.mem_write(OUT,bytes([165])*28)
 status=call(x,entry,[VIEW,1,B,struct.unpack('<I',data[8:12])[0],OUT,OUT+24]);assert status!=0 and bytes(x.mem_read(OUT,28))==bytes([165])*28
 assert subprocess.check_output(probe,input=data)==w(status)+bytes([165])*28
report=dict(guards=len(guards),result='PASS',original_pc_nxdk_samples=len(inputs),exhausted=modes[0],vertex=modes[1],edges=modes[2],passes=passes,original_sha256=sha,x87_control_word='0x027f',scope='Original4f3720 through selection and interpolation, actual linked topology and normal array/vector helpers. Observation stops before accumulation or failure RGB writes. Caller supplies radius and retained smoothed normals; coverage, ownership and live lighting resources excluded.')
(root/'artifacts/lightmap-select-sample.json').write_text(json.dumps(report,indent=2));print(report)
