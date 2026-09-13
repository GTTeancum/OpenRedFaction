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
entry=int(re.search(r'\s_rf_lightmap_mapping_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
inputs=[];outputs=[];rng=random.Random(0x4ee210);fallbacks=0
for case in range(2048):
 count=1+case%7;image=rng.choice([0,count-1,count,0xffffffff,0x80000000]);fallbacks+=image>=count
 raw=bytearray(96);struct.pack_into('<I',raw,0,image);raw[4:8]=bytes(rng.randrange(256) for _ in range(4))
 for offset in [8,12,16,20,24,28,32,36,40,44,48,52,76,80,84,88]:struct.pack_into('<f',raw,offset,rng.uniform(-100,100))
 for offset in [56,60,64,68,72,92]:struct.pack_into('<I',raw,offset,rng.choice([0,1,2,3,0xffffffff,0x80000000]))
 record=bytes(raw);cursor=0;trace.clear();u.mem_write(b,bytes([165])*124)
 u.mem_write(0x14f2408,w(count,count,array));u.mem_write(array,w(*(0x12340000+j for j in range(count))));u.mem_write(stack,bytes(256))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EDI,b);u.reg_write(UC_X86_REG_ESI,b+0x8000);u.reg_write(UC_X86_REG_EBX,1);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x4ee2db,0x4ee51d,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4ee51d and cursor==96
 expected=w(struct.unpack('<I',u.mem_read(b+12,4))[0]-0x12340000)+bytes(u.mem_read(b+16,16))+bytes(u.mem_read(b+44,8))+bytes(u.mem_read(b+52,24))+bytes(u.mem_read(b+108,16))+w(u.mem_read(b+9,1)[0],u.mem_read(b+10,1)[0])+bytes(u.mem_read(b+92,12))+bytes(u.mem_read(b+76,8))+bytes(u.mem_read(b+84,8))+bytes(u.mem_read(b+104,4))
 assert len(expected)==108
 x.mem_write(b,record);x.mem_write(out,bytes([165])*108);x.mem_write(stack,w(stop,b,96,count,out));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(out,108))==expected,(case,bytes(x.mem_read(out,108)).hex(),expected.hex());inputs.append(w(count)+record);outputs.append(w(0)+expected)
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-mapping-read'],input=b''.join(inputs))==b''.join(outputs)
for size,count in [(95,1),(97,1),(96,0)]:
 x.mem_write(out,bytes([165])*108);x.mem_write(stack,w(stop,b,size,count,out));x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=10000)
 assert x.reg_read(UC_X86_REG_EAX)!=0 and bytes(x.mem_read(out,108))==bytes([165])*108
report=dict(result='PASS',original_pc_nxdk_records=2048,image_fallbacks=fallbacks,nxdk_guards=3,original_sha256=digest,scope='Original4ee2db..4ee51d version180 mapping loader, typed file/version services supplied. All finite numeric fields, rectangle bytes, boolean normalization, axes, room and image-index fallback match complete108-byte PC/NXDK records. Allocation, authored inventory, image ownership and nonfinite reader behavior excluded.')
(root/'artifacts/lightmap-mapping-read.json').write_text(json.dumps(report,indent=2));print(report)
