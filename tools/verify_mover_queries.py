"""Real loaded mover ray/sphere queries vs complete original and NXDK flat path."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
base=0x30000000;solid=base;faces=base+4096;verts=base+0x100000;edges=base+0x200000;query=base+0x400000;out=query+4096;stack=base+0x700000;stop=stack+4096
u=Uc(UC_ARCH_X86,UC_MODE_32);p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im);u.mem_map(base,0x800000)
x=Uc(UC_ARCH_X86,UC_MODE_32);xp=pefile.PE(str(root/'build/xbox/main.exe'));xi=xp.get_memory_mapped_image();xb=xp.OPTIONAL_HEADER.ImageBase;x.mem_map(xb,(len(xi)+4095)//4096*4096);x.mem_write(xb,xi);x.mem_map(base,0x800000)
entry=int(re.search(r'_rf_collision_flat_faces\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def run(vm,entry,args,ecx=0):
 vm.mem_write(stack,struct.pack('<'+'I'*(len(args)+1),stop,*args));vm.reg_write(UC_X86_REG_ESP,stack);vm.reg_write(UC_X86_REG_ECX,ecx);vm.reg_write(UC_X86_REG_FPCW,0x37f);vm.emu_start(entry,stop,count=3000000);assert vm.reg_read(UC_X86_REG_EIP)==stop
levels=json.loads((root/'artifacts/movers.json').read_text())['results'];results=[]
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-faces',str(root/'Installed_Game'/level['archive']),level['file']]);movers,peak=struct.unpack_from('<2I',raw);at=8;wires=[];expected=[];hits=multi=edge_hits=0
 for mover in range(movers):
  count,=struct.unpack_from('<I',raw,at);at+=4;records=[];vpos=verts;epos=edges
  assert count*128<0xff000
  u.mem_write(solid,bytes(256));u.mem_write(solid+0x70,struct.pack('<I',faces if count else 0))
  for i in range(count):
   index,n=struct.unpack_from('<2I',raw,at);head=raw[at+8:at+48];metadata=raw[at+48:at+72];v=raw[at+72:at+72+n*12];at+=72+n*12;assert index==i
   _,flags,portal,present,kind,state=struct.unpack('<IIiIII',metadata);assert not present
   f=faces+i*128;u.mem_write(f,bytes(128));u.mem_write(f,head);u.mem_write(f+0x28,struct.pack('<I',flags));u.mem_write(f+0x30,struct.pack('<i',-1));u.mem_write(f+0x34,struct.pack('<h',portal));u.mem_write(f+0x40,struct.pack('<I',epos));u.mem_write(f+0x54,struct.pack('<I',f+128 if i+1<count else 0));u.mem_write(vpos,v)
   for j in range(n):u.mem_write(epos+j*32,struct.pack('<I',vpos+j*12)+bytes(16)+struct.pack('<II',epos+((j+1)%n)*32,epos+((j-1)%n)*32))
   x.mem_write(faces+i*72,head+struct.pack('<II',vpos,n)+metadata);x.mem_write(vpos,v)
   records.append((struct.unpack('<4f',head[:16]),list(struct.iter_unpack('<3f',v))))
   vpos+=n*12;epos+=n*32
  assert epos<query and vpos<edges
  for i,(plane,points) in enumerate(records):
   for mode in range(3):
    anchor=[sum(p[j] for p in points)/len(points) for j in range(3)] if mode==0 else list(points[0]) if mode==1 else [(points[0][j]+points[1][j])*.5 for j in range(3)]
    start=[anchor[j]+plane[j]+.0037*(j+1) for j in range(3)];delta=[-2*plane[j]+.0013*(j+1) for j in range(3)];radius=[0,.25,.75][mode];flags=[0x464,0x465,0x1464][mode];limit=1
    wire=struct.pack('<2I20f',mover,flags,*start,*delta,0,0,0,1,0,0,0,1,0,0,0,1,radius,limit);wires.append(wire)
    u.mem_write(query,bytes(128));u.mem_write(query+4,wire[32:80]);u.mem_write(query+0x34,wire[8:32]+wire[80:84]+struct.pack('<I',flags));u.mem_write(out,struct.pack('<If',0,limit)+bytes(32));u.mem_write(0xca06e0,bytes(8));u.mem_write(0xca06b0,struct.pack('<I',15));u.mem_write(0x1754525,b'\x03');u.mem_write(0x1754558,bytes(12));u.mem_write(0x1754488,bytes(12))
    run(u,0x4df1c0,[query,out,0],solid)
    updates,=struct.unpack('<I',u.mem_read(out,4));hit=int(updates>0);hits+=hit;multi+=updates>1
    if hit:
     edge,index=struct.unpack('<2I',u.mem_read(out+32,8));assert (index-faces)%128==0;index=(index-faces)//128;assert index<count;edge_hits+=bool(edge);value=bytes(u.mem_read(out+4,28))+struct.pack('<3I',index,updates,edge)
    else:value=bytes([0xa5])*40
    want=struct.pack('<iI',0,hit)+value;expected.append(want)
    x.mem_write(query,wire);x.mem_write(out,bytes([0xa5])*44);run(x,entry,[faces,count,flags,query+8,query+20,query+32,query+44,struct.unpack_from('<I',wire,80)[0],struct.unpack_from('<I',wire,84)[0],out,out+40])
    got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out+40,4))+bytes(x.mem_read(out,40));assert got==want,('NXDK',level['file'],mover,i,mode,got.hex(),want.hex())
 assert at==len(raw)
 got=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--mover-queries',str(root/'Installed_Game'/level['archive']),level['file']],input=b''.join(wires));assert len(got)==len(expected)*48
 for i,want in enumerate(expected):assert got[i*48:i*48+48]==want,('PC',level['file'],i,got[i*48:i*48+48].hex(),want.hex())
 results.append(dict(file=level['file'],queries=len(wires),hits=hits,multiple_updates=multi,edge_hits=edge_hits));print(level['file'],len(wires),flush=True)
report=dict(result='PASS',levels=len(results),queries=sum(r['queries'] for r in results),hits=sum(r['hits'] for r in results),multiple_updates=sum(r['multiple_updates'] for r in results),edge_hits=sum(r['edge_hits'] for r in results),scope='Three local queries per installed mover face: centroid thin ray, first vertex .25 sphere, first edge midpoint .75 sphere. Full original uncached 4df1c0 flat path with all callees unchanged vs PC owned movers after source closure and NXDK function in Unicorn. Initial file metadata, identity/direct poses; no runtime mover creation, changing poses, material checks or XEMU.',results=results)
(root/'artifacts/mover-query-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
