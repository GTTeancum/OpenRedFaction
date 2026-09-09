"""Run complete original model lighting setup with supplied globals and lights."""
import hashlib,json,math,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,65536);esp=base+32000;model=base+256;pos=base+512;rotation=base+600;stop=base+60000;objects=[base+4096+i*256 for i in range(8)]
u.mem_write(0xc9687c,struct.pack('<I',8));u.mem_write(0xc4d588,struct.pack('<8I',*objects))
rng=random.Random(520);cases=[];expected=[]
for n in range(2000):
    flags=0x400 if n%2 else 0;alternate=(n//2)%2;disabled=(n//4)%2
    color=bytes(rng.randrange(256) for _ in range(4));ambient=struct.pack('<3f',*(rng.random() for _ in range(3)));gain=struct.pack('<f',rng.uniform(0,2))
    matrices=[struct.pack('<9f',*(rng.randint(-16,16)/16 for _ in range(9))) for _ in range(2)]
    position=struct.pack('<3f',*(rng.randint(-16,16)/4 for _ in range(3)))
    payload=struct.pack('<3I',flags,alternate,disabled)+color+ambient+gain+b''.join(matrices)+position
    records=[];colors=[]
    for i,obj in enumerate(objects):
        record=struct.pack('<4fI',*(rng.randint(-16,16)/4 for _ in range(3)),rng.uniform(1,100),rng.randrange(2))
        rgb=struct.pack('<3f',*(rng.random() for _ in range(3)));records.append(record);colors.append(rgb)
        u.mem_write(obj,bytes(256));u.mem_write(obj+12,record[:12]);u.mem_write(obj+0x80,record[12:16]);u.mem_write(obj+0x4d,bytes([struct.unpack_from('<I',record,16)[0]]));u.mem_write(obj+0x40,rgb)
    cases.append(payload+b''.join(records)+b''.join(colors))
    u.mem_write(model,bytes(128));u.mem_write(model,struct.pack('<I',flags));u.mem_write(model+0x28,color);u.mem_write(model+0x2c,matrices[0]);u.mem_write(pos,position);u.mem_write(rotation,matrices[1])
    u.mem_write(0x1c45258,bytes([alternate]));u.mem_write(0x1c45254,struct.pack('<I',disabled));u.mem_write(0x5a38d4,ambient);u.mem_write(0x5a6fb4,gain)
    u.mem_write(esp,struct.pack('<4I',stop,model,pos,rotation));u.reg_write(UC_X86_REG_ESP,esp)
    u.emu_start(0x52dad0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(bytes(u.mem_read(0x1c3d500,84)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--lighting-setup'],input=b''.join(cases))
assert len(actual)==len(cases)*84
exact=0;maximum_error=0
for n,reference in enumerate(expected):
    observed=actual[n*84:n*84+84];exact+=observed==reference
    for i,(a,b) in enumerate(zip(struct.unpack('<21f',observed),struct.unpack('<21f',reference))):
        if math.isnan(b):assert math.isnan(a)
        elif math.isinf(b):assert a==b
        else:
            maximum_error=max(maximum_error,abs(a-b));assert abs(a-b)<=max(1e-5,abs(b)*2e-6),(n,i,a,b)
report=dict(result='PASS',cases=len(cases),bit_exact_cases=exact,max_absolute_error=maximum_error,
    scope='Complete original 0x52dad0 and unchanged callees with supplied globals/model/light list; both alternate/disable/flag modes; 2e-6 relative/1e-5 absolute tolerance; world light discovery and renderer integration excluded')
(root/'artifacts/model-lighting-setup-verification.json').write_text(json.dumps(report,indent=2));print(report)
