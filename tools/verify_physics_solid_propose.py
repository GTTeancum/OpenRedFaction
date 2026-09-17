"""Execute original generic-body translation49f9c3..49fbe6; no patched callees."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/python'))
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=ROOT/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image()
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;u.mem_map(base,0x10000)
pack=lambda v:struct.pack('<%df'%len(v),*v)
rng=random.Random(0x49f930);commands=[];expected=[]
for case in range(512):
    flags=(case%4)|(0x1000000 if case&4 else 0);objects=0x80000 if case&8 else 0
    values=[(1,5,100,800)[(case//64)%4],(0,1/60,1/30,.1)[(case//16)%4],(9.8,0,-3)[case%3],(0,.01,.5,2)[(case//64)%4]]
    values += [rng.uniform(-100,100) for _ in range(3)]
    values += [rng.uniform(-5,5) for _ in range(3)]
    values += [rng.uniform(-20,20) for _ in range(3)]
    values += [rng.uniform(-3,3) for _ in range(3)]
    command=struct.pack('<II',flags,objects)+pack(values);values=struct.unpack('<16f',command[8:])
    u.mem_write(base,bytes(0x1500));u.mem_write(stack-0x200,bytes(0x400))
    for offset,items in [(0x98,values[:1]),(0x1b0,values[1:2]),(0x8c,values[3:4]),(0xe4,values[4:7]),(0x144,values[7:10]),(0x168,values[10:13])]:u.mem_write(base+offset,pack(items))
    u.mem_write(base+0x1a8,struct.pack('<I',flags));u.mem_write(base+0x7c,struct.pack('<I',objects))
    u.mem_write(0x5a00dc,pack(values[2:3]));u.mem_write(0x7c7048,pack(values[13:16]))
    u.mem_write(stack+0x9c,struct.pack('<I',base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x49f9c3,0x49fbe6,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x49fbe6
    commands.append(command);expected.append(bytes(u.mem_read(base+0x144,12))+bytes(u.mem_read(base+0xf0,12))+bytes(u.mem_read(0x7c7048,12)))
actual=subprocess.check_output([str(ROOT/'build/pc/Release/rf_physics_probe.exe'),'--solid-propose'],input=b''.join(commands))
for i,want in enumerate(expected):
    assert actual[i*36:(i+1)*36]==want,('PC mismatch',i,actual[i*36:(i+1)*36].hex(),want.hex())
native_cases=0
if '--nxdk' in sys.argv:
    pe=pefile.PE(str(ROOT/'build/xbox/main.exe'));binary=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(binary)+4095)//4096*4096);x.mem_write(origin,binary);x.mem_map(base,0x10000)
    entry=int(re.search(r'_rf_physics_solid_propose\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
    for case,command in enumerate(commands):
        flags,objects=struct.unpack('<II',command[:8]);values=struct.unpack('<16f',command[8:]);state=bytearray([0xa5]*308)
        for offset,items in [(12,values[:1]),(4,values[3:4]),(88,values[4:7]),(184,values[7:10]),(220,values[10:13])]:state[offset:offset+len(items)*4]=pack(items)
        state[272:276]=struct.pack('<I',flags);x.mem_write(base,bytes(state));x.mem_write(base+0x2000,pack(values[13:16]));stop=base+0xf000
        x.mem_write(stack,struct.pack('<IIffII',stop,base,values[1],values[2],objects,base+0x2000));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
        x.emu_start(entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
        state[184:196]=expected[case][:12];state[100:112]=expected[case][12:24]
        assert bytes(x.mem_read(base,308))==state,('NXDK body mismatch',case)
        assert bytes(x.mem_read(base+0x2000,12))==expected[case][24:],('NXDK acceleration mismatch',case)
        native_cases+=1
report=dict(result='PASS',cases=len(expected),nxdk_cases=native_cases,scope='Original generic solid translation with gravity flags, midpoint liquid drag and repeat-pass acceleration; rotation/contact/live scheduling excluded')
(ROOT/'artifacts/physics-solid-propose.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
