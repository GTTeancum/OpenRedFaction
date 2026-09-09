"""Check movemodes bindings and full original translation transform 433a50."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX,UC_X86_REG_ESI
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));image=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
f=lambda *v:struct.pack('<%df'%len(v),*v)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
def names(address,count):
    return [p.get_string_at_rva(struct.unpack('<I',p.get_data(address-0x400000+i*4,4))[0]-0x400000).decode() for i in range(count)]
mode_names=names(0x596384,16);refs=[names(0x5963c4,4),names(0x5963d4,4)]
b=(root/'Installed_Game/tables.vpp').read_bytes();n=struct.unpack_from('<I',b,8)[0];at=2048+((n*64+2047)&~2047)
for i in range(n):
    offset=2048+i*64;name=b[offset:offset+60].split(b'\0')[0];size=struct.unpack_from('<I',b,offset+60)[0]
    if name==b'movemodes.tbl':table=b[at:at+size].decode();break
    at+=(size+2047)&~2047
table=re.sub(r'//[^\n]*','',table);descriptors=[]
for index,name in enumerate(mode_names):
    block=re.search(r'\$name:\s*"'+re.escape(name)+r'"(.*?)(?=\$name:|#end)',table,re.S|re.I)[1]
    fields=[refs[kind].index(re.search(r'\$'+kind_name+r' '+axis+r' ref:\s*"([^"]+)"',block,re.I)[1]) for kind,kind_name in enumerate(('move','rot')) for axis in 'xyz']
    expected=w(1,index,*fields)
    actual=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--descriptor',str(root/'Installed_Game/tables.vpp'),str(index)])
    assert actual==expected,('Descriptor differs from authored values/original name tables',name)
    descriptors.append(dict(name=name,words=list(struct.unpack('<8I',actual))))
rng=random.Random(0x433a50);commands=[];expected=[]
for case in range(256):
    reference=[case%4,(case//4)%4,(case//16)%4] if case<64 else [rng.choice([0,1,2,3,4,0xffffffff]) for _ in range(3)]
    command=w(*reference)+f(*[rng.uniform(-3,3) for _ in range(30)]);commands.append(command)
    u.mem_write(base,bytes(0x6000));u.mem_write(base+0x858,w(base+0x2000));u.mem_write(base+0x85c,w(base+0x3000))
    u.mem_write(base+0x2008,command[:12]);u.mem_write(base+0x4000,command[12:24])
    u.mem_write(base+0x7e0,command[24:60]);u.mem_write(base+0xfc,command[60:96]);u.mem_write(base+0x3000,command[96:132])
    u.mem_write(stack,w(stop,base+0x5000,base,base+0x4000));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x433a50,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(bytes(u.mem_read(base+0x5000,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--transform'],input=b''.join(commands))
assert actual==b''.join(expected),'PC transform differs from original'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
entry=int(re.search(r'_rf_movement_transform\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(commands):
    x.mem_write(base,command);x.mem_write(stack,w(stop,base,base+12,base+24,base+60,base+96,base+0x5000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base+0x5000,12))==expected[case],('NXDK transform differs',case)
accel_commands=[];accel_expected=[]
for case,command in enumerate(commands):
    acceleration=(0,1,20,100)[case%4];command+=f(acceleration);accel_commands.append(command)
    u.mem_write(base,bytes(0x6000));u.mem_write(base+0x858,w(base+0x2000));u.mem_write(base+0x85c,w(base+0x3000))
    u.mem_write(base+0x294,w(base+0x2100));u.mem_write(base+0x215c,f(acceleration))
    u.mem_write(base+0x2008,command[:12]);u.mem_write(base+0x714,command[12:24])
    u.mem_write(base+0x7e0,command[24:60]);u.mem_write(base+0xfc,command[60:96]);u.mem_write(base+0x3000,command[96:132])
    u.mem_write(stack,bytes(0x100));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49f6cd,0x49f753,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49f753
    accel_expected.append(bytes(u.mem_read(stack+0x10,12)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--acceleration'],input=b''.join(accel_commands))
assert actual==b''.join(accel_expected),'PC acceleration differs from original'
entry=int(re.search(r'_rf_movement_acceleration\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
for case,command in enumerate(accel_commands):
    x.mem_write(base,command);x.mem_write(stack,w(stop,base,base+12)+command[132:136]+w(base+24,base+60,base+96,base+0x5000))
    x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base+0x5000,12))==accel_expected[case],('NXDK acceleration differs',case)
miner=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe'),'--class',str(root/'Installed_Game/tables.vpp'),'miner1'])
assert miner==f(6,.5,1.5,20),'Miner movement values differ from authored block'
fixture=root/'artifacts/movement-class-tests/fixture.vpp';fixture.parent.mkdir(parents=True,exist_ok=True)
fixtures=[('$Name: "test"\n$Max Vel: 6\n$Acceleration: 20\n',f(6,1,1,20)),
          ('$Name: "test"\n$Max Vel: 6\n+fast factor: 1.5\n$Acceleration: 20\n',f(6,1,1.5,20)),
          ('$Name: "test"\n$Max Vel: 6\n',None),
          ('$Name: "test"\n$Max Vel: 6\n$Acceleration: -1\n',None),
          ('$Name: "test"\n$Max Vel: 6\n+slow factor: nope\n$Acceleration: 20\n',None)]
for text,wanted in fixtures:
    payload=text.encode();length=4096+((len(payload)+2047)&~2047);archive=bytearray(length)
    archive[:16]=w(0x51890ace,1,1,length);archive[2048:2058]=b'entity.tbl';archive[2108:2112]=w(len(payload));archive[4096:4096+len(payload)]=payload;fixture.write_bytes(archive)
    checked=subprocess.run([str(root/'build/pc/Release/rf_movement_probe.exe'),'--class',str(fixture),'test'],capture_output=True)
    assert (checked.returncode==0 and checked.stdout==wanted) if wanted is not None else (checked.returncode!=0 and not checked.stdout)
report=dict(status='PASS',original_pc_nxdk_transform_cases=256,original_pc_nxdk_acceleration_cases=256,class_parser_fixtures=len(fixtures),miner_movement_values=list(struct.unpack('<4f',miner)),authored_descriptors=descriptors,scope='Complete original 433a50 and prepared 49f6cd..49f753 with unchanged callees. Parser bindings independently compared with installed table and original reference/name arrays; original file parser not executed.')
(root/'artifacts/movement-transform-verification.json').write_text(json.dumps(report,indent=2));print('PASS: 256 original/PC/NXDK transforms, 256 acceleration clamps, 16 authored descriptors and miner movement values')
