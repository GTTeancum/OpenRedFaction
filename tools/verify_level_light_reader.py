"""All installed v180 light records: independent inventory vs PC/NXDK reader."""
import ctypes as c,json,re,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
class Light(c.Structure):
 _fields_=[(n,c.c_uint32) for n in ('offset','bytes','uid')]+[(n,c.c_char*256) for n in ('name','script')]+[('position',c.c_float*3),('orientation_disk',c.c_float*9),('header_byte',c.c_uint32),('flags',c.c_uint32),('color',c.c_uint8*4)]+[(n,c.c_float) for n in ('radius','inner_angle','outer_delta','cone_scale')]+[('profile',c.c_uint32),('length',c.c_float),('cycle',c.c_float*6)]
w=lambda *v:struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]);B=0x30000000;LEVEL=B+0x8000;OUT=B+0x1000;STACK=B+0xe000;STOP=B+0xf000
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase;x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(ib,(len(im)+4095)//4096*4096);x.mem_write(ib,im);x.mem_map(B,65536)
mp=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'_'+n+r'\s+([0-9a-fA-F]+)',mp)[1],16);read_entry=sym('rf_vpp_read');data=b''
def hook(cpu,address,size,context):
 sp=cpu.reg_read(UC_X86_REG_ESP);ret,archive,entry,offset,dest,count=struct.unpack('<6I',cpu.mem_read(sp,24));offset-=8
 assert offset<=len(data) and count<=len(data)-offset
 if count:cpu.mem_write(dest,data[offset:offset+count])
 cpu.reg_write(UC_X86_REG_EAX,0);cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,ret)
x.hook_add(UC_HOOK_CODE,hook,begin=read_entry,end=read_entry)
def call(name,args):
 x.mem_write(STACK,w(STOP,*args));x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(sym(name),STOP,count=1000000);assert x.reg_read(UC_X86_REG_EIP)==STOP;return x.reg_read(UC_X86_REG_EAX)
inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());report=json.loads((root/'artifacts/level-lights.json').read_text());total=0
for level in report['results']:
 description=next(l for l in levels if l['file']==level['file'] and l['archive']==level['archive']);section=next(s for s in description['sections'] if s['type']=='0x300');archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
 with (root/'Installed_Game'/level['archive']).open('rb') as stream:stream.seek(entry['offset']+section['offset']+8);data=stream.read(section['size'])
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-lights',str(root/'Installed_Game'/level['archive']),level['file']]);assert len(raw)==len(level['records'])*c.sizeof(Light)
 x.mem_write(LEVEL,bytes(4096));x.mem_write(LEVEL,w(B+0x9000));x.mem_write(LEVEL+72,w(len(data)+8,180,0,1));x.mem_write(LEVEL+600,w(0x300,0,len(data)))
 assert call('rf_level_lights_begin',[LEVEL,B])==0
 for i,reference in enumerate(level['records']):
  expected=Light()
  for name,typ in Light._fields_:
   value=reference[name]
   if typ==c.c_char*256:setattr(expected,name,value.encode('cp1252'))
   elif issubclass(typ,c.Array):getattr(expected,name)[:]=value
   else:setattr(expected,name,value)
  want=bytes(expected);assert raw[i*len(want):(i+1)*len(want)]==want,(level['file'],i,'PC')
  before=bytes(x.mem_read(B,28));x.mem_write(B+12,w(reference['offset']+reference['bytes']-1));saved=bytes(x.mem_read(B,28));x.mem_write(OUT,b'\xa5'*len(want))
  assert call('rf_level_light_next',[B,OUT])!=0 and bytes(x.mem_read(B,28))==saved and bytes(x.mem_read(OUT,len(want)))==b'\xa5'*len(want)
  x.mem_write(B,before);assert call('rf_level_light_next',[B,OUT])==0 and bytes(x.mem_read(OUT,len(want)))==want,(level['file'],i,'NXDK');total+=1
 assert call('rf_level_light_next',[B,OUT])!=0 and struct.unpack('<I',x.mem_read(B+16,4))[0]==len(data)
result=dict(result='PASS',levels=len(report['results']),pc_nxdk_records=total,pc_nxdk_truncated_records=total,record_bytes=c.sizeof(Light),scope='Original45f260 static reader sequence, independent Python inventory, exact PC/NXDK decoded fields and section exhaustion. NXDK substitutes only archive read; each final-byte truncation preserves reader/output. Original loader execution/runtime conversion/native creation are separate.')
(root/'artifacts/level-light-reader-verification.json').write_text(json.dumps(result,indent=2));print(result)
