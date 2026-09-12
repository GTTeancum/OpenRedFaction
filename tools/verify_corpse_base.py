"""Compare concrete registered corpse base owners with original486da0."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
f=lambda *v:struct.pack('<'+'f'*len(v),*v)
commands=[];expected=[]
def observe(v):
 commands.append(f(10,v['mass'],*v['position'],*v['basis'],v['radius'])+w(v['flags'])+f(.25,.5,2)+w(v['count'],v['generation'],v['slot'],v['room'])+v['spheres'])
 read=v['read'];a=v['actor'];u=v['u']
 expected.append(w(read(a),read(a+0x7c))+bytes(u.mem_read(a+4,12))+w(*[read(a+o) for o in (0x2c,0x34,0x7c,0x80,0x78,0x180,0x1a8,0x26c,0x1fc,0x2cc)])+bytes(u.mem_read(a+0x3c,48))+v['state']+w(v['used'])+v['owned'])
runpy.run_path(str(root/'tools/verify_corpse_base_original.py'),init_globals={'observe_case':observe})
pc=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-base'],input=b''.join(commands));assert pc==b''.join(expected),'PC base mismatch'
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);b=0x30000000;u.mem_map(b,0x40000)
registry=b+0x8000;seed=b+0xc000;source=b+0xc100;out=b+0xc200;head=b+0xc300;countp=b+0xc400;stack=b+0x3e000;stop=b+0x3f000;heap=b+0x10000;body=b+136+276;c=b+136
mapping=(root/'build/xbox/main.map').read_text();sym=lambda n:int(re.search(r'\s_'+n+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
init=sym('rf_corpse_owners_init');reginit=sym('rf_object_registry_init');entry=sym('rf_corpse_base_acquire');recycle=sym('rf_corpse_owners_recycle');remove=sym('rf_object_registry_remove');malloc=sym('malloc');free=sym('free')
read=lambda a:struct.unpack('<I',u.mem_read(a,4))[0]
fail=False;live=False;calls=[]
def hook(cpu,address,size,data):
 global live
 if address not in (malloc,free):return
 sp=cpu.reg_read(UC_X86_REG_ESP);arg=read(sp+4);calls.append((address,arg))
 if address==malloc:
  assert 0<arg<=96 and not live
  live=not fail
  if live:cpu.mem_write(heap,bytes([0xa5])*arg)
  cpu.reg_write(UC_X86_REG_EAX,heap if live else 0)
 else:assert arg==heap and live;live=False
 cpu.reg_write(UC_X86_REG_ESP,sp+4);cpu.reg_write(UC_X86_REG_EIP,read(sp))
u.hook_add(UC_HOOK_CODE,hook)
def call(address,*args):
 u.mem_write(stack,w(stop,*args));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(address,stop,count=1000000);assert u.reg_read(UC_X86_REG_EIP)==stop;return u.reg_read(UC_X86_REG_EAX)
for case,(wire,want) in enumerate(zip(commands,expected)):
 values=struct.unpack('<23I',wire[:92]);used=values[19] or 1;budget=19224+used*24
 assert call(init,b,budget)==0;call(reginit,registry)
 u.mem_write(registry+12288,w(values[21],1,values[20]));u.mem_write(c,bytes([0xa5])*276)
 u.mem_write(seed,wire[:60]+w(source,values[19],values[15]));u.mem_write(source,wire[92:] or bytes(24))
 previous=head+16 if case%2 else head
 u.mem_write(head,w(previous,previous));u.mem_write(previous,w(head,head));u.mem_write(countp,w(case%2));u.mem_write(out,w(99));calls.clear()
 args=(b,registry,head,countp,seed,*values[16:19],values[22],out)
 # Exhausted handles reject before reserving a pool slot or allocating memory.
 u.mem_write(registry+12292,w(0));assert call(entry,*args)==0xfffffffd and not calls and read(out)==99 and read(b+124)==0
 u.mem_write(registry+12292,w(1));fail=True
 assert call(entry,*args)==0xfffffffc and not live and read(out)==99 and read(b+124)==0 and read(registry+12296)==values[20] and read(registry+12292)==1
 fail=False;calls.clear();assert call(entry,*args)==0 and read(out)==0
 assert calls==[(malloc,used*24)] and read(countp)==1+case%2
 handle=read(c+116);assert handle==struct.unpack_from('<I',want,20)[0]
 assert read(registry+values[21]*8)==c and read(registry+values[21]*8+4)==handle and read(registry+12292)==0
 assert read(registry+12296)==(1 if values[20]==0x752e else values[20]+1)
 assert read(head+4)==read(previous)==c+100 and bytes(u.mem_read(c+100,8))==w(head,previous)
 assert bytes(u.mem_read(c+92,8))==bytes(8) and read(c+88)==c and read(c+124)==c and read(c+120)==read(c+108)==0
 got=bytes(u.mem_read(c+600,20))+w(*[read(c+offset) for offset in (116,0,8,28,172,176,208,132,140,36)])+bytes(u.mem_read(c+40,48))+bytes(u.mem_read(body,308))+w(read(body+312))+bytes(u.mem_read(read(body+308),used*24))
 assert got==want,('NXDK base',case)
 assert bytes(u.mem_read(seed,60))==wire[:60] # immutable caller seed, including negative radius
 # Fixture teardown: no constructor effects have run.
 u.mem_write(head,w(head,head));assert call(remove,registry,handle)==0 and call(recycle,b,0)==0 and not live
report=dict(result='PASS',cases=len(commands),heap_failure_cases=len(commands),exhausted_registry_cases=len(commands),nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Represented base fields/body bytes vs full original486da0; real shared owner allocation, registry generation and object list binding. PC/NXDK, accepted room tokens bound; no native XEMU or live room search/string/model binding. Preserves incoming sound; negative seed radius is normalized in a local copy.')
(root/'artifacts/corpse-base-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
