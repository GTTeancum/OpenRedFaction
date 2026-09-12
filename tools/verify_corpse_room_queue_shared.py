"""Compare shared corpse room collection with the full original collector."""
import hashlib,json,re,runpy,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
binary=root/'build/xbox/main.exe';p=pefile.PE(str(binary));data=p.get_memory_mapped_image();base=p.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(base,(len(data)+4095)//4096*4096);x.mem_write(base,data)
b=0x30000000;x.mem_map(b,0x30000);pool=b+0x1000;queue=b+0x1100;frustum=b+0x1200;count_address=b+0x1300;records=b+0x10000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'\s_rf_corpse_surface_collect_room\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
inputs=[];outputs=[]
def observe(active,room,count,planes,offset,nodes,after,expected_records):
 x.mem_write(b,nodes);x.mem_write(pool,w(0,b if active else 0,8));x.mem_write(records,bytes([0xa5])*98304);x.mem_write(count_address,w(count))
 f=bytearray(156);struct.pack_into('<f',f,0,1);struct.pack_into('<I',f,144,planes);x.mem_write(frustum,bytes(f))
 x.mem_write(queue,w(frustum)+struct.pack('<3f',*offset)+w(records,count_address,2048,0x42df20))
 x.mem_write(stack,w(stop,pool,room,queue));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
 assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(count_address,4))+bytes(x.mem_read(records,98304));expected=w(0,after)+expected_records
 assert actual==expected,('NXDK',len(inputs))
 assert bytes(x.mem_read(b,len(nodes)))==nodes
 inputs.append(w(active,room,count,planes)+struct.pack('<3f',*offset)+nodes);outputs.append(expected)
runpy.run_path(str(root/'tools/verify_corpse_room_queue_original.py'),init_globals={'observe_case':observe})
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--corpse-surface-collect'],input=b''.join(inputs))
assert actual==b''.join(outputs),'PC differs'
report=dict(result='PASS',cases=len(inputs),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope='Shared room collector and queue append vs unhooked original42e140/4d3560/5186a0.288 cases, exact count and all2048 records; PC object pointers normalized, NXDK same synthetic node addresses. Effect payload and active links preserved. Supplied world offsets, no instance transforms. Does not run draw callback or live campaign collection.')
(root/'artifacts/corpse-room-queue-shared.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
