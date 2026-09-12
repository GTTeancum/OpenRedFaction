"""Execute compiled NXDK resource loading with supplied archive reads and heap."""
import json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
B=0x30000000;N=B+0x1000;M=B+0x2000;O=B+0x5000;S=B+0x700000;STOP=S+0x1000
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,0x800000)
mapping=(ROOT/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
entry,close,open_file,find,read_file,malloc,calloc,free=map(sym,('rf_static_render_resource_open','rf_static_render_resource_close','rf_model_file_open','rf_vpp_find','rf_vpp_read','malloc','calloc','free'))
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v));r=lambda a:struct.unpack('<I',x.mem_read(a,4))[0]
live={};allocations=reads=0;fail_alloc=fail_read=0;cursor=B+0x10000;raw=b'';filename='';cases=0

def hook(cpu,a,size,ctx):
 global allocations,reads,cursor
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(sp+4+i*4);status=0
 if a in (malloc,calloc):
  allocations+=1;count=arg(0)*(arg(1) if a==calloc else 1)
  if allocations!=fail_alloc:
   status=cursor;cursor+=(count+15)&~15;assert cursor<S;live[status]=count;cpu.mem_write(status,bytes(count) if a==calloc else b'\xa5'*count)
 elif a==free:
  if arg(0):count=live.pop(arg(0));cpu.mem_write(arg(0),b'\xdd'*count)
 elif a==find:cpu.mem_write(arg(2),filename.encode().ljust(64,b'\0')+w(0,len(raw)))
 else:
  reads+=1;offset,count=arg(2),arg(4)
  if reads==fail_read:status=0xffffffff
  else:
   assert offset+count<=len(raw),(filename,offset,count,len(raw));cpu.mem_write(arg(3),raw[offset:offset+count])
 cpu.reg_write(UC_X86_REG_EAX,status);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,arg(-1))
for a in (find,read_file,malloc,calloc,free):x.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,*args):
 x.mem_write(S,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,S);x.emu_start(a,STOP,count=100000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
def hash_bytes(data):
 h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 return h
def digest():
 parts,lods,materials,np,nl,nm,nb=struct.unpack('<7I',x.mem_read(O,28));out=[f'R 0 {np} {nl} {nm} {nb}',f'H {hash_bytes(x.mem_read(parts,np*48))} {hash_bytes(x.mem_read(materials,nm*84))}']
 for i in range(nl):
  g=lods+i*40;batches,vertices,triangles,reuse,bc,vc,tc,gb,planes,threshold=struct.unpack('<10I',x.mem_read(g,40))
  out.extend((f'P {i} {threshold} {hash_bytes(x.mem_read(planes,tc*16))}',f'G {i} {bc} {vc} {tc} {gb}'))
  for j in range(bc):
   fv,nv,ft,nt,mat=struct.unpack('<5I',x.mem_read(batches+j*20,20))
   out.extend((f'V {i} {j} {hash_bytes(x.mem_read(vertices+fv*40,nv*40))} {hash_bytes(x.mem_read(triangles+ft*8,nt*8))} {hash_bytes(x.mem_read(reuse+fv*4,nv*4))}',f'M {i} {j} {mat}'))
 assert nb==44+sum(live.values()),(filename,nb,live)
 return out
def check(budget,wanted,pc):
 global allocations,reads,cursor,cases
 assert not live;allocations=reads=0;cursor=B+0x10000;x.mem_write(O,bytes(44));status=call(entry,M,budget,O)
 assert status==wanted,(filename,status,wanted)
 if status:assert bytes(x.mem_read(O,44))==bytes(44) and not live
 else:assert digest()==pc,filename
 total_allocations,total_reads=allocations,reads
 call(close,O);call(close,O);assert bytes(x.mem_read(O,44))==bytes(44) and not live;cases+=1
 return total_allocations,total_reads
names=['RunwayLight01_Larger','Double_MineLight04','minelight03','vines01','Port_Light01','shard04','shard05','Veg_Plant01','Veg_Plant02','shard02','shard03','shard01']
archive=next(a for a in json.loads((ROOT/'artifacts/inventory.json').read_text())['files'] if a['path']=='meshes.vpp');entries={e['name'].lower():e for e in archive['vpp']['entries']};rows=[]
for name in names:
 e=entries[name.lower()+'.v3m'];filename=e['name']
 with (ROOT/'Installed_Game/meshes.vpp').open('rb') as f:f.seek(e['offset']);raw=f.read(e['size'])
 x.mem_write(N,filename.encode()+b'\0');assert call(open_file,M,B,N)==0
 pc=subprocess.check_output([str(ROOT/'build/pc/Release/rf_model_file_probe.exe'),str(ROOT/'Installed_Game/meshes.vpp'),filename,'--static-resource','4194304'],text=True).splitlines();budget=int(pc[0].split()[-1])
 count,read_count=check(budget,0,pc);check(budget-1,0xfffffffc,pc)
 if not rows:
  for fail_alloc in range(1,count+1):check(budget,0xffffffff,pc)
  fail_alloc=0
  for fail_read in (1,read_count//2,read_count):check(budget,0xffffffff,pc)
  fail_read=0
 rows.append(dict(model=filename,bytes=budget,allocations=count,reads=read_count));print(rows[-1],flush=True)
report=dict(result='PASS',cases=cases,models=len(rows),retained=sum(a['bytes'] for a in rows),scope='Compiled NXDK actual file decoder/resource composition; only VPP find/read and heap supplied. Opening12 models versus PC retained bytes/hashes, exact/short budgets, every first-model allocation failure and three read failures, full cleanup. No native XEMU/texture or live scene claim.',records=rows)
(ROOT/'artifacts/static-render-resource-nxdk.json').write_text(json.dumps(report,indent=2));print(report)
