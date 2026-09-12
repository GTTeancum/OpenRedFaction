"""Audit opening authored face material sampling against original UV/bitmap code."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(a&0xffffffff for a in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(im)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,im);u.mem_map(0x30000000,0x10000);return u
u=machine(exe);x=machine(root/'build/xbox/main.exe');entry=int(re.search(r'\s_rf_geometry_material_sample\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
symbols=(root/'build/xbox/main.map').read_text();sym=lambda name:int(re.search(r'\s_'+name+r'\s+([0-9a-fA-F]+)',symbols)[1],16)
bind=sym('rf_geometry_material_collision_bind');sample=sym('rf_geometry_material_collision_sample')
B=0x30000000;DATA=B+0x1000;OUT=B+0x2000;S=B+0xe000;STOP=B+0xf000
lock=0;trace=[];width=height=pitch=fmt=0
r=lambda cpu,a:struct.unpack('<I',cpu.mem_read(a,4))[0]
def hook(cpu,address,size,context):
 global lock
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=lambda i:r(cpu,sp+4+i*4)
 if address==0x55ce00:
  assert arg(0)==17 and arg(1)==0;lock=arg(2);cpu.mem_write(lock,w(0,0,fmt,DATA,width,height,pitch,0));cpu.reg_write(UC_X86_REG_EAX,1);trace.append('lock')
 else:assert arg(0)==lock;trace.append('release')
 cpu.reg_write(UC_X86_REG_EIP,r(cpu,sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x55ce00,0x50e310):u.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def run(cpu,fn,args):
 cpu.mem_write(S,w(STOP,*args));cpu.reg_write(UC_X86_REG_ESP,S);cw=0x27f if cpu is x else 0x37f;cpu.reg_write(UC_X86_REG_FPCW,cw);cpu.emu_start(fn,STOP,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==STOP;assert cpu.reg_read(UC_X86_REG_FPCW)==cw;return cpu.reg_read(UC_X86_REG_EAX)
DATA=B+0x9000;OUT=B+0xa000;VERT=B+0x3000;EDGE=B+0x4000;POINT=B+0x7000;WORK=B+0x6000;IMAGE=B+0x8000
for cpu in (u,x):cpu.mem_map(B+0x10000,0x2000000-0x10000)
DATA=B+0x100000;MATERIALS=B+0x20000;SLOTS=B+0x21000;OFFSETS=SLOTS+0x1000;MAPPING=OFFSETS+0x100;results=[];total=uv_misses=bounds=guards=mapping_guards=missing_faces=0;adapter_guards=0
levels=json.loads((root/'artifacts/geometry.json').read_text())[:1]
for level in levels:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-indexed-material-samples',str(root/'Installed_Game'/level['archive']),level['file'],*[str(root/'Installed_Game'/name) for name in ('maps_en.vpp','maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp')]]);at=0;counted=0
 local_count=struct.unpack_from('<I',raw,at)[0];at+=4;slots=struct.unpack_from('<'+'I'*local_count,raw,at);at+=4*local_count;image_count=struct.unpack_from('<I',raw,at)[0];at+=4;images=[];address=DATA
 for slot in range(image_count):
  status,width,height,size,fmt=struct.unpack_from('<i4I',raw,at);at+=20;pixels=raw[at:at+size];at+=size;assert status in (0,-3)
  if status:
   assert size==0;x.mem_write(MATERIALS+slot*28,w(0,0,0,0,0,status,0));images.append((1,1,0,bytes(4),status));continue
  assert size==width*height*4
  native=bytearray(size);locked=bytearray();bpp=1 if fmt==2 else 2 if fmt in (4,5) else 4
  for yy in range(height):
   for xx in range(width):
    offset=(yy*width+xx)*4;r0,g0,b0,a0=pixels[offset:offset+4]
    if fmt==2:locked+=bytes([a0])
    elif fmt==4:locked+=struct.pack('<H',((a0>>4)<<12)|((r0>>4)<<8)|((g0>>4)<<4)|(b0>>4))
    elif fmt==5:locked+=struct.pack('<H',((a0>>7)<<15)|((r0>>3)<<10)|((g0>>3)<<5)|(b0>>3))
    else:locked+=bytes((b0,g0,r0,a0))
    index=0;bit=0
    for level_bit in range(max(width.bit_length(),height.bit_length())-1):
     if 1<<level_bit<width:index|=((xx>>level_bit)&1)<<bit;bit+=1
     if 1<<level_bit<height:index|=((yy>>level_bit)&1)<<bit;bit+=1
    native[index*4:index*4+4]=pixels[offset:offset+4]
  assert address+size<B+0x2000000;x.mem_write(address,bytes(native));x.mem_write(MATERIALS+slot*28,w(width,height,size,fmt,address,status,0));images.append((width,height,fmt,bytes(locked),status));address+=(size+4095)//4096*4096
 x.mem_write(SLOTS,w(*slots));x.mem_write(OFFSETS,w(0,local_count));x.mem_write(MAPPING,w(MATERIALS,image_count,image_count,0,0,OFFSETS,SLOTS,1,0,0))
 while at<len(raw):
  texture=struct.unpack_from('<I',raw,at)[0];at+=4
  assert texture<local_count;width,height,fmt,locked,missing_status=images[slots[texture]];pitch=width*(1 if fmt==2 else 2 if fmt in (4,5) else 4);u.mem_write(DATA,locked+bytes(16))
  index,count=struct.unpack_from('<2I',raw,at);assert index==counted and 3<=count<=256;plane=raw[at+8:at+24];point=raw[at+24:at+36];vertices=raw[at+36:at+36+count*12];coords=raw[at+36+count*12:at+36+count*20];actual=raw[at+36+count*20:at+60+count*20];at+=60+count*20
  u.mem_write(B,plane+bytes(64));u.mem_write(B+0x40,w(EDGE));u.mem_write(VERT,vertices);u.mem_write(POINT,point);u.mem_write(OUT,b'\xa5'*8)
  for i in range(count):u.mem_write(EDGE+i*32,w(VERT+i*12)+coords[i*8:i*8+8]+bytes(8)+w(EDGE+(i+1)%count*32,EDGE+(i-1)%count*32)+bytes(4))
  u.reg_write(UC_X86_REG_ECX,B);matched=run(u,0x4e1ad0,(POINT,OUT,OUT+4))&255;uv=bytes(u.mem_read(OUT,8));want=w(0,matched)+uv
  color=w(0xa5a5a5a5)
  if missing_status:status=missing_status;missing_faces+=1
  elif not matched:status=-3;uv_misses+=1
  else:
   aa,bb=struct.unpack('<2f',uv);aa=math.fmod(aa,1);bb=math.fmod(bb,1);aa+=1 if aa<0 else 0;bb+=1 if bb<0 else 0
   if fmt in (2,4,5,7) and int(bb*height+.5)*width+int(aa*width+.5)>=width*height:status=-4;bounds+=1
   else:
    trace=[];run(u,0x55cfa0,(17,*struct.unpack('<2I',uv),OUT));assert trace==['lock','release'];color=bytes(u.mem_read(OUT,4));status=0
  want+=w(status)+color;assert actual==want,(level['file'],index,'PC',actual.hex(),want.hex())
  face=plane+w(texture,0xffffffff)+bytes(28)+w(count);assert len(face)==56
  data=vertices+face+b''.join(w(i)+coords[i*8:i*8+8] for i in range(count));x.mem_write(VERT,data);x.mem_write(B+0x1000,w(len(vertices)));g=bytearray(68)
  struct.pack_into('<I',g,0,VERT);struct.pack_into('<I',g,4,len(data));struct.pack_into('<I',g,12,local_count);struct.pack_into('<I',g,20,count);struct.pack_into('<I',g,24,1);struct.pack_into('<I',g,56,B+0x1000);x.mem_write(B,bytes(g));x.mem_write(POINT,point);x.mem_write(WORK,w(B+0x4000,B+0x5000,256));x.mem_write(OUT,w(0xa5a5a5a5))
  VIEW=B+0x23000;SOURCES=VIEW+64;BITMAPS=VIEW+128;BACKEND=VIEW+192;FACE=VIEW+256
  x.mem_write(VIEW,w(MAPPING,B,SOURCES,3,0,WORK));x.mem_write(SOURCES,w(0,0,0));x.mem_write(BACKEND,b'\xa5'*12)
  assert run(x,bind,(VIEW,BITMAPS,3,BACKEND))==0
  assert bytes(x.mem_read(BACKEND,12))==w(BITMAPS,sample,VIEW)
  assert bytes(x.mem_read(BITMAPS,12))==w(*([slots[texture]]*3))
  got=w(run(x,sample,(VIEW,index%3,FACE,slots[texture],POINT,OUT)))+bytes(x.mem_read(OUT,4));assert got==want[16:],(level['file'],index,'NXDK',got.hex(),want[16:].hex());assert bytes(x.mem_read(VERT,len(data)))==data
  if index==0:
   for bad_index,bad_bitmap,expected_status in [(3,slots[texture],-4),(0,image_count,-2),(0,0xffffffff,-2)]:
    x.mem_write(OUT,w(0xa5a5a5a5));assert w(run(x,sample,(VIEW,bad_index,FACE,bad_bitmap,POINT,OUT)))+bytes(x.mem_read(OUT,4))==w(expected_status,0xa5a5a5a5);adapter_guards+=1
   x.mem_write(BACKEND,b'\xa5'*12);assert w(run(x,bind,(VIEW,BITMAPS,2,BACKEND)))==w(-4) and bytes(x.mem_read(BACKEND,12))==b'\xa5'*12;adapter_guards+=1
   x.mem_write(SOURCES,w(99));x.mem_write(OUT,w(0xa5a5a5a5));assert w(run(x,sample,(VIEW,0,FACE,slots[texture],POINT,OUT)))+bytes(x.mem_read(OUT,4))==w(-4,0xa5a5a5a5);adapter_guards+=1
   assert w(run(x,bind,(VIEW,BITMAPS,3,BACKEND)))==w(-4) and bytes(x.mem_read(BACKEND,12))==b'\xa5'*12;adapter_guards+=1
   x.mem_write(WORK+8,w(count-1));x.mem_write(OUT,w(0xa5a5a5a5));assert w(run(x,entry,(MAPPING,0,B,0,POINT,WORK,OUT)))+bytes(x.mem_read(OUT,4))==w(-4,0xa5a5a5a5);guards+=1
   x.mem_write(WORK+8,w(256))
   # Invalid ownership/mapping must fail before a pixel read and preserve output.
   args=(MAPPING,0,B,0,POINT,WORK,OUT)
   def guard(expected,changes=(),arguments=args):
    global mapping_guards
    saved=[(where,bytes(x.mem_read(where,len(value)))) for where,value in changes]
    for where,value in changes:x.mem_write(where,value)
    x.mem_write(OUT,w(0xa5a5a5a5))
    assert w(run(x,entry,arguments))+bytes(x.mem_read(OUT,4))==w(expected,0xa5a5a5a5)
    for where,value in saved:x.mem_write(where,value)
    mapping_guards+=1
   guard(-4,arguments=(0,*args[1:]))
   guard(-4,arguments=(MAPPING,1,*args[2:]))
   guard(-4,changes=((MAPPING+20,w(0)),))
   guard(-2,changes=((OFFSETS,w(local_count+1)),))
   guard(-2,changes=((OFFSETS+4,w(local_count-1)),))
   guard(-2,changes=((MAPPING+24,w(0)),))
   guard(-2,changes=((SLOTS+texture*4,w(image_count)),))
   guard(-2,changes=((MAPPING,w(0)),))
   guard(-1,changes=((MATERIALS+slots[texture]*28+20,w(-1)),))
   guard(-3,changes=((VERT+len(vertices)+16,w(0xffffffff)),))
   guard(-2,changes=((VERT+len(vertices)+16,w(local_count)),))
  counted+=1;total+=1
 assert at==len(raw) and counted==level['faces'];results.append(dict(file=level['file'],faces=counted))
report=dict(result='PASS',adapter_guards=adapter_guards,faces=total,images=image_count,loaded_images=sum(im[4]==0 for im in images),missing_images=sum(im[4]!=0 for im in images),missing_faces=missing_faces,uv_misses=uv_misses,bounds_rejections=bounds,nxdk_workspace_guards=guards,nxdk_mapping_guards=mapping_guards,levels=results,original_sha256=sha,scope='Opening level authored geometry and actual retained material textures. Full original4e1ad0 and55cfa0 with only lock/release supplied, source channel precision restored from normalized decoded images. PC reverse source-face mapping and compiled NXDK binder/indexed sampler with repeated source-face references with distinct swizzled image allocations match. Adapter adds no allocation; existing workspace and bitmap scratch supplied. Static current images only; no live collision traversal or animation-selection claim.')
(root/'artifacts/geometry-indexed-material-sample.json').write_text(json.dumps(report,indent=2));print(report)
