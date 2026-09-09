"""Original named class sphere override loop versus shared PC/NXDK."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW
pack=lambda *v:struct.pack('<'+'I'*len(v),*v)
floats=lambda *v:struct.pack('<'+'f'*len(v),*v)
name=lambda n:n.encode('ascii').ljust(24,bytes(1))
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));b=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(b)+4095)//4096*4096);u.mem_write(0x400000,b)
base=0x30000000;cls=base+0x2000;handle=base+0x5000;model=base+0x6000;source=base+0x7000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
names=['csphere_0','csphere_1','csphere_2','csphere_20','Sphere','sphere','other','last']
queries=['CSPHERE_0','csphere_2','csphere','SPHERE','missing','','last','csphere_20']
commands=[];expected=[];rng=random.Random(0x423e7e)
for case in range(270):
 count=case%9;over=(case//9)%9;network=(case//81)%3
 state=b''.join(name(names[i])+floats(1,2,3,4,-1)+pack(0xaabb0000+i)+floats(i,.5,-i)+pack(i) for i in range(count))
 override=b''.join(name(queries[(j+case)%8])+floats((-1,0,.25,float('nan'))[(j+case)%4],j+.5,j+1.5,(-1,0,.75,float('nan'))[(case+j//2)%4])+pack(0x12340000+j) for j in range(over))
 declared=b''.join(override[j*44+24:j*44+28]+pack(0xa5a5a5a5)+override[j*44+28:j*44+44]+override[j*44:j*44+24] for j in range(over))
 u.mem_write(base,bytes(0x1000));u.mem_write(base+0x29c,pack(cls));u.mem_write(base+0x80,pack(handle));u.mem_write(handle,pack(1,model));u.mem_write(model+0x60,pack(count,source))
 u.mem_write(cls,bytes([0xa5])*0x1514);u.mem_write(cls+0xb68,pack(over)+declared);u.mem_write(cls+0xcec,pack(count)+b''.join(state[j*64+24:(j+1)*64] for j in range(count)))
 u.mem_write(source,b''.join(name(n)+bytes(20) for n in names));u.mem_write(0x64ecb9,bytes([network]))
 before=bytes(u.mem_read(cls,0x1514));u.mem_write(stack,pack(stop));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_FPCW,0x37f)
 u.emu_start(0x423e7e,0x42405d,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x42405d
 result=b''.join(state[j*64:j*64+24]+bytes(u.mem_read(cls+0xcf0+j*40,40)) for j in range(count))
 after=bytearray(before);after[0xcf0:0xcf0+count*40]=b''.join(result[j*64+24:(j+1)*64] for j in range(count));assert bytes(u.mem_read(cls,0x1514))==after
 commands.append(pack(count,over,network)+state+override);expected.append(result)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_probe.exe'),'--sphere-overrides'],input=b''.join(commands));assert actual==b''.join(expected),'PC override mismatch'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_entity_sphere_overrides\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for i,(command,want) in enumerate(zip(commands,expected)):
 count,over,network=struct.unpack_from('<3I',command);x.mem_write(base,command[12:12+count*64] or bytes(64));x.mem_write(source,command[12+count*64:] or bytes(44))
 x.mem_write(stack,pack(stop,base,count,source,over,network));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x37f)
 x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
 assert bytes(x.mem_read(base,count*64))==want,('NXDK override',i)
report=dict(result='PASS',original_pc_nxdk_cases=len(commands),scope='Complete original 423e7e..42405d including model sphere prefix lookup and ASCII comparison, no hooks. Counts 0..8, ordered/duplicate/missing/empty/prefix/mixed-case queries, positive/zero/negative/NaN optional fields, network bytes 0/1/2. Entire class descriptor checked outside target records. Not table parsing, default sphere construction or live entity integration.')
(root/'artifacts/entity-sphere-overrides-verification.json').write_text(json.dumps(report,indent=2));print(report)
