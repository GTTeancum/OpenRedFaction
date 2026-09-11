"""Execute original48ac70 and cached skeletal callees vs PC/NXDK sound point."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<'+'I'*len(v),*(v&0xffffffff for v in v))
def machine(path):
 p=pefile.PE(str(path));im=p.get_memory_mapped_image();ib=p.OPTIONAL_HEADER.ImageBase
 u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(ib,(len(im)+4095)//4096*4096);u.mem_write(ib,im);u.mem_map(0x30000000,65536);return u
original=root/'Installed_Game/RF.exe';digest=hashlib.sha256(original.read_bytes()).hexdigest()
assert digest=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(original);x=machine(root/'build/xbox/main.exe');b=0x30000000
desc=b+0x4000;handle=b+0x5000;actor=b+0x6000;output=b+0xa000;stack=b+0xe000;stop=b+0xf000
entry=int(re.search(r'_rf_model_sound_follow_point\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
rng=random.Random(0x48ac70);cases=[];expected=[]
for i in range(4096):
 index=i%5-1
 values=[rng.uniform(-32,32) for _ in range(48)]+[rng.uniform(-2,2) for _ in range(9)]+[rng.uniform(-10000,10000) for _ in range(3)]
 if i%7==0:values[48:57]=[1,0,0,0,1,0,0,0,1]
 wire=w(index)+struct.pack('<60f',*values)
 # Fallback must not inspect absent model/basis, and must preserve raw position.
 if index==-1 and i%2==0:wire=wire[:196]+w(*([0x7fc00000]*9))+w(0x80000000,0x7fc12345,0x7f800000)
 cases.append(wire)
 u.mem_write(b,bytes(0x3000));u.mem_write(b,wire[4:196]);u.mem_write(b+0x1d50,w(desc));u.mem_write(desc+0x48,w(4));u.mem_write(handle,w(2,b))
 u.mem_write(actor,bytes(0x300));u.mem_write(actor+0x3c,wire[232:244]);u.mem_write(actor+0x48,wire[196:232]);u.mem_write(actor+0x80,w(0 if index==-1 else handle));u.mem_write(actor+0x26c,w(index))
 u.mem_write(output,bytes([0xa5])*12);u.mem_write(stack,w(stop,actor,output));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x48ac70,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
 assert (u.reg_read(UC_X86_REG_EAX)&255)==int(index!=-1)
 expected.append(w(0)+bytes(u.mem_read(output,12)))
original_count=len(cases)
for index in (-2,4):cases.append(w(index)+cases[1][4:]);expected.append(w(-4)+bytes([0xa5])*12)
for offset in (4,40,196,232):
 wire=bytearray(cases[1]);wire[offset:offset+4]=w(0x7fc00000);cases.append(bytes(wire));expected.append(w(-2)+bytes([0xa5])*12)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--sound-follow'],input=b''.join(cases))
assert len(actual)==16*len(cases)
for i,(wire,want) in enumerate(zip(cases,expected)):
 assert actual[i*16:(i+1)*16]==want,('PC',i,actual[i*16:(i+1)*16].hex(),want.hex())
 index=struct.unpack_from('<i',wire)[0];x.mem_write(b,wire[4:]);x.mem_write(output,bytes([0xa5])*12)
 x.mem_write(stack,w(stop,0 if index==-1 else b,4,index,0 if index==-1 else b+192,b+228,output))
 x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
 assert w(x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(output,12))==want,('NXDK',i)
report=dict(result='PASS',original_cases=original_count,guard_cases=len(cases)-original_count,original_sha256=digest,nxdk_sha256=hashlib.sha256((root/'build/xbox/main.exe').read_bytes()).hexdigest(),scope='Complete unhooked48ac70 with cached kind2 bone lookup and4fb9d0 transform; PC/NXDK bit-exact output including absent-model fallback. No lazy evaluation, virtual attachments or live corpse sound integration.')
(root/'artifacts/sound-follow-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
