"""Compare retained real-level room lookup against original 4e1630."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_FPCW
if sys.argv[1:]==['--all']:
 results=[];levels=json.loads((ROOT/'artifacts/geometry.json').read_text())
 for index,entry in enumerate(levels):
  subprocess.run([sys.executable,__file__,entry['archive'],entry['file']],check=True,stdout=subprocess.DEVNULL)
  record=json.loads((ROOT/('artifacts/loaded-room-locator-'+entry['file']+'.json')).read_text());record.pop('results');results.append(record)
  if index%10==0:print(index+1,len(levels),entry['file'],flush=True)
 report=dict(result='PASS',levels=len(results),queries=sum(r['queries'] for r in results),nxdk_queries=sum(r['nxdk_queries'] for r in results),owned=sum(r['owned'] for r in results),retries=sum(r['retries'] for r in results),results=results)
 (ROOT/'artifacts/loaded-room-locator-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'});raise SystemExit(0)
archive=sys.argv[1] if len(sys.argv)>1 else 'levels1.vpp';level=sys.argv[2] if len(sys.argv)>2 else 'L1S1.rfl'
wire=subprocess.check_output([str(ROOT/'build/pc/Release/rf_collision_probe.exe'),'--world-locate-dump',str(ROOT/'Installed_Game'/archive),level]);offset=0
def take(n):
 global offset
 out=wire[offset:offset+n];assert len(out)==n;offset+=n;return out
def ints(n):return struct.unpack('<'+'I'*n,take(n*4))
room_count,primary_count=ints(2);bounds=take(24);primary=ints(primary_count);rooms=[]
for r in range(room_count):
 skip,nodes,faces=ints(3);ns=[take(40) for _ in range(nodes)];fs=[]
 for f in range(faces):
  geometry=take(40);count,flags,source=ints(3);vertices=take(count*12);fs.append((geometry,count,flags,source,vertices))
 rooms.append((skip,ns,fs))
queries=[]
for _ in range(ints(1)[0]):queries.append((take(12),ints(3)))
assert offset==len(wire)
exe=ROOT/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));raw=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(raw)+4095)//4096*4096);u.mem_write(0x400000,raw)
base=0x30000000;u.mem_map(base,64*1024*1024);cursor=base
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def allocate(n):
 global cursor
 out=cursor;cursor+=(n+15)//16*16;assert cursor<base+60*1024*1024;return out
solid=allocate(256);roomptrs=[allocate(512) for _ in rooms];plist=allocate(primary_count*4);u.mem_write(plist,w(*[roomptrs[i] for i in primary]));u.mem_write(solid+0x48,bounds);u.mem_write(solid+0x9c,w(primary_count,primary_count,plist));face_ids={}
for r,(skip,nodes,faces) in enumerate(rooms):
 rp=roomptrs[r];u.mem_write(rp+1,bytes([skip]));np=[allocate(64) for _ in nodes];fp=[allocate(128) for _ in faces]
 u.mem_write(rp+0x3c,w(np[0] if np else 0));u.mem_write(rp+0x28,w(fp[0] if fp else 0))
 for i,(geometry,count,flags,source,vertices) in enumerate(faces):
  face=fp[i];face_ids[face]=source;vp=allocate(count*12);ep=[allocate(32) for _ in range(count)];u.mem_write(vp,vertices)
  u.mem_write(face,geometry+w(flags));u.mem_write(face+0x40,w(ep[0] if ep else 0,rp));u.mem_write(face+0x5c,w(fp[i+1] if i+1<len(fp) else 0))
  for j in range(count):u.mem_write(ep[j],w(vp+j*12,0,0,0,0,ep[(j+1)%count],ep[(j-1)%count]))
 for i,node in enumerate(nodes):
  first,count,left,right=struct.unpack('<4I',node[24:]);u.mem_write(np[i],node[:24]+w(fp[first] if count else 0));u.mem_write(np[i]+0x20,w(np[left] if left!=0xffffffff else 0,np[right] if right!=0xffffffff else 0))
  for j in range(count):u.mem_write(fp[first+j]+0x58,w(fp[first+j+1] if j+1<count else 0))
point=allocate(16);stack=base+63*1024*1024;stop=stack+0x10000;results=[]
for i,(position,wanted) in enumerate(queries):
 u.mem_write(point,position);u.mem_write(stack,w(stop,solid,point));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4e1630,stop,count=10000000);assert u.reg_read(UC_X86_REG_EIP)==stop,('instruction limit',i)
 owner=u.reg_read(UC_X86_REG_EAX);selected=struct.unpack('<I',u.mem_read(stack-0x1014,4))[0];retries=struct.unpack('<I',u.mem_read(stack-0x1054,4))[0]
 actual=(roomptrs.index(owner),face_ids[selected],retries) if owner else (0xffffffff,0xffffffff,retries)
 assert actual==wanted,('lookup mismatch',level,i,struct.unpack('<3f',position),actual,wanted)
 results.append(dict(position=struct.unpack('<3f',position),room=actual[0],face=actual[1],retries=retries))
# Materialize the same retained data in the compiled NXDK layout and execute
# the source-ID mapping wrapper, independently of the original pointer layout.
p=pefile.PE(str(ROOT/'build/xbox/main.exe'));raw=p.get_memory_mapped_image();x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(raw)+4095)//4096*4096);x.mem_write(p.OPTIONAL_HEADER.ImageBase,raw);x.mem_map(base,64*1024*1024);cursor=base
entry=int(re.search(r'_rf_geometry_collision_world_locate\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
world=allocate(64);owned=allocate(room_count*80);views=allocate(room_count*40);plist=allocate(primary_count*4)
x.mem_write(plist,w(*primary));x.mem_write(world,w(0,owned,views,plist,0,room_count,primary_count,0,0,0)+bounds)
for r,(skip,nodes,faces) in enumerate(rooms):
 np=allocate(len(nodes)*40);fp=allocate(len(faces)*72);sources=allocate(len(faces)*4);scratch=allocate(len(nodes)*4)
 if nodes:x.mem_write(np,b''.join(nodes))
 x.mem_write(owned+r*80,w(0,np,fp,sources,scratch,len(nodes),len(faces),len(nodes),0,0))
 x.mem_write(views+r*40,bytes(24)+w(skip,0,0,owned+r*80))
 for i,(geometry,count,flags,source,vertices) in enumerate(faces):
  vp=allocate(len(vertices));x.mem_write(vp,vertices);x.mem_write(fp+i*72,geometry+w(vp,count,0,flags,0,0,0,0));x.mem_write(sources+i*4,w(source))
point=allocate(16);output=allocate(16)
for i,(position,wanted) in enumerate(queries):
 x.mem_write(point,position);x.mem_write(stack,w(stop,world,point,output));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=10000000);assert x.reg_read(UC_X86_REG_EIP)==stop,('NXDK instruction limit',i)
 assert x.reg_read(UC_X86_REG_EAX)==0,('NXDK error',i)
 assert tuple(struct.unpack('<3I',x.mem_read(output,12)))==wanted,('NXDK lookup mismatch',level,i)
report=dict(result='PASS',archive=archive,level=level,rooms=room_count,primary=primary_count,queries=len(results),nxdk_queries=len(results),owned=sum(r['room']!=0xffffffff for r in results),retries=sum(r['retries'] for r in results),scope='Complete original 4e1630 over materialized retained real-level trees and face/vertex data. Original loader is not executed; ownership and ordering come from the reconstructed loader. PC queries run after source closure/poison allocation and repeat identically.',results=results)
(ROOT/('artifacts/loaded-room-locator-'+level+'.json')).write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
