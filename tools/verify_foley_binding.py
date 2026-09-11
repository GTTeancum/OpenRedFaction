"""Original class footstep assignment block versus shared PC/NXDK binding."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe';digest=hashlib.sha256(exe.read_bytes()).hexdigest();assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));im=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(im)+4095)//4096*4096);u.mem_write(0x400000,im)
base=0x30000000;u.mem_map(base,0x40000);cls=base;names_ptr=base+0x10000;stack=base+0x30000;stop=0x41c7e8
index=0;name_count=0;missing=False

def hook(uc,address,size,context):
 global index,missing
 if address==0x41c833:
  missing=True;uc.reg_write(UC_X86_REG_EIP,stop);return
 if address not in (0x5125c0,0x512bb0,0x4ff480):return
 sp=uc.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',uc.mem_read(sp,4))[0];pop=0;result=0
 if address==0x5125c0:result=int(index<name_count);pop=4
 elif address==0x512bb0:pop=12
 else:result=names_ptr+index*32;index+=1
 uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ESP,sp+4+pop);uc.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x41c781);cases=[]
for k in range(160):
 group_count=k%13
 group_names=[('group'+str(i%7)).encode() for i in range(group_count)]
 records=b''.join(struct.pack('<32sIII',name,rng.randrange(10),i,0) for i,name in enumerate(group_names))
 selections=[rng.choice(group_names).swapcase() for _ in range(k%17)] if group_names else []
 if k%9==0:selections+=[b'missing']
 if k%11==0:selections+=[b'']
 names=b''.join(n.ljust(32,b'\0') for n in selections)
 cases.append(struct.pack('<II',group_count,len(selections))+records+names)
wire=b''.join(cases);out=subprocess.run([str(root/'build/pc/Release/rf_audio_probe.exe'),'--foley-bind'],input=wire,capture_output=True,check=True).stdout;assert len(out)==len(cases)*44
failed=0
for k,raw in enumerate(cases):
 ng,name_count=struct.unpack_from('<II',raw);index=0;missing=False
 u.mem_write(0x636ef8,struct.pack('<I',ng));u.mem_write(0x6300f8,raw[8:8+ng*44]);u.mem_write(names_ptr,raw[8+ng*44:]);u.mem_write(cls,bytes(0x200))
 u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_EBP,cls);u.reg_write(UC_X86_REG_EBX,base+0x2000);u.reg_write(UC_X86_REG_ESI,0xffffffff)
 u.emu_start(0x41c781,stop,count=2000000);assert u.reg_read(UC_X86_REG_EIP)==stop
 expected=struct.pack('<i',-3 if missing else 0)+(bytes([0xa5])*40 if missing else bytes(u.mem_read(cls+0x178,40)))
 assert out[k*44:(k+1)*44]==expected,(k,missing,out[k*44:(k+1)*44].hex(),expected.hex());failed+=missing
p=pefile.PE(str(root/'build/xbox/main.exe'));im=p.get_memory_mapped_image();nx=Uc(UC_ARCH_X86,UC_MODE_32);b=p.OPTIONAL_HEADER.ImageBase;nx.mem_map(b,(len(im)+4095)//4096*4096);nx.mem_write(b,im);nx.mem_map(base,0x40000)
entry=int(re.search(r'\s_rf_foley_bind_materials\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);native_stop=base+0x3f000
for k,raw in enumerate(cases):
 ng,nn=struct.unpack_from('<II',raw);nx.mem_write(base+0x1000,raw[8:]);nx.mem_write(base,struct.pack('<6I',base+0x1000,0,ng,0,0,0));nx.mem_write(base+0x20000,bytes([0xa5])*40)
 nx.mem_write(stack,struct.pack('<5I',native_stop,base,base+0x1000+ng*44,nn,base+0x20000));nx.reg_write(UC_X86_REG_ESP,stack)
 nx.emu_start(entry,native_stop,count=2000000);assert nx.reg_read(UC_X86_REG_EIP)==native_stop
 actual=struct.pack('<I',nx.reg_read(UC_X86_REG_EAX))+bytes(nx.mem_read(base+0x20000,40));assert actual==out[k*44:(k+1)*44],k
report=dict(result='PASS',cases=len(cases),missing_name_cases=failed,original_sha256=digest,scope='Original41c781..41c7e8 initializes and assigns material slots; actual434cb0/434d70 and name comparison execute. Parser/string access supplied; missing-name fatal branch intercepted. PC/NXDK match successful slots and port error-preservation policy. No full entity-table parse or class lifecycle integration.')
(root/'artifacts/foley-binding.json').write_text(json.dumps(report,indent=2));print(report)
