"""Original version180 mapping loader slice with sequential typed file reads."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));data=p.get_memory_mapped_image();m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096);m.mem_write(p.OPTIONAL_HEADER.ImageBase,data);m.mem_map(0x30000000,65536);return m
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);binary=root/'build/xbox/main.exe';x=machine(binary)
b=0x30000000;array=b+0x1000;out=b+0x2000;tmp=b+0x3000;stub=b+0x4000;stack=b+0xe000;stop=b+0xf000
u.mem_write(stub,b'\xd9\x05'+w(tmp)+b'\xc2\x08\x00')
record=b'';cursor=0;trace=[]
def hook(m,address,size,unused):
 global cursor
 sp=m.reg_read(UC_X86_REG_ESP);get=lambda a:struct.unpack('<I',m.mem_read(a,4))[0]
 def ret(value,pop):
  m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,get(sp))
 if address==0x4f02c0:ret(180,0);return
 readers={0x514e10:(4,0),0x5155a0:(1,8),0x514e50:(4,8),0x514f90:(12,12),0x5153c0:(4,8)}
 if address not in readers:return
 size,pop=readers[address];data=record[cursor:cursor+size];assert len(data)==size;trace.append((address,cursor,size));cursor+=size
 if address==0x514e50:m.mem_write(tmp,data);m.reg_write(UC_X86_REG_EIP,stub)
 elif address==0x514f90:m.mem_write(get(sp+4),data);ret(get(sp+4),pop)
 else:ret(int.from_bytes(data,'little'),pop)
u.hook_add(UC_HOOK_CODE,hook)
entry=int(re.search(r'\s_rf_lightmap_projection_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[];rng=random.Random(0x4ee2db)
def shared(raw,want):
 x.mem_write(b,raw);x.mem_write(out,bytes([0xa5])*24);x.mem_write(stack,w(stop,b,96,out));x.reg_write(UC_X86_REG_ESP,stack)
 x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
 actual=w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(out,24));assert actual==want,(len(inputs),actual.hex(),want.hex())
 inputs.append(raw);outputs.append(want)
for case in range(1024):
 raw=bytearray(96);struct.pack_into('<I',raw,0,case%3);raw[4:8]=bytes([1,2,3,4])
 for offset in range(8,96,4):struct.pack_into('<f',raw,offset,rng.uniform(-100,100))
 struct.pack_into('<2I',raw,68,case%3,(case//3)%3)
 record=bytes(raw);cursor=0;trace.clear();u.mem_write(b,bytes([0xa5])*124)
 u.mem_write(0x14f2408,w(3,3,array));u.mem_write(array,w(0x12340000,0x12340001,0x12340002));u.mem_write(stack,bytes(256))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,b);u.reg_write(UC_X86_REG_ESI,b+0x8000);u.reg_write(UC_X86_REG_EBX,1);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4ee2db,0x4ee51d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4ee51d and cursor==96,(hex(u.reg_read(UC_X86_REG_EIP)),cursor)
 assert bytes(u.mem_read(b+12,4))==w(0x12340000+case%3)
 projection=bytes(u.mem_read(b+0x60,8))+bytes(u.mem_read(b+0x4c,8))+bytes(u.mem_read(b+0x54,8))
 assert projection==record[68:76]+record[84:92]+record[76:84]
 shared(record,w(0)+projection)
for offset,bits in [(68,3),(72,0xffffffff),(76,0x7fc00000),(88,0x7f800000)]:
 raw=bytearray(inputs[0]);struct.pack_into('<I',raw,offset,bits);shared(bytes(raw),w(-2)+bytes([0xa5])*24)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--lightmap-projection-read'],input=b''.join(inputs));assert actual==b''.join(outputs),'PC mismatch'
report=dict(result='PASS',original_cases=1024,port_guards=4,original_sha256=digest,nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),typed_reads=trace,scope='Original4ee2db..4ee51d version180 loader slice; sequential typed file reads and version query supplied. Original stores/branches/index resolution and exact96-byte consumption checked. Shared PC/NXDK projection extraction matches. Allocation, file-reader implementations, non180 versions, invalid original records and authored mapping inventory are not covered.')
(root/'artifacts/lightmap-projection-read.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='typed_reads'},indent=2))
