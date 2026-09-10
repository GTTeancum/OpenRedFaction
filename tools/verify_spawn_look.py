"""Compare spawn-look C with the complete original factory angle span."""
import hashlib,json,math,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
BASE=0x30000000;STACK=BASE+0xe000;STOP=BASE+0xf000
def machine(path):
    p=pefile.PE(str(path));image=p.get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(p.OPTIONAL_HEADER.ImageBase,(len(image)+4095)//4096*4096);u.mem_write(p.OPTIONAL_HEADER.ImageBase,image)
    u.mem_map(BASE,65536);u.reg_write(UC_X86_REG_FPCW,0x37f);return u
exe=ROOT/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
u=machine(exe);inputs=[];expected=[]
starts=json.loads((ROOT/'artifacts/player-start-verification.json').read_text())['levels']
for case in starts:
    matrix=struct.pack('<9I',*case['transform_words'][3:]);inputs.append(matrix+matrix+struct.pack('<3I',1,3,0))
identity=[1,0,0,0,1,0,0,0,1]
for x,y,z in ((0,0,0),(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)):
    matrix=identity[:6]+[x,y,z]
    inputs.append(struct.pack('<18f3I',*(matrix+identity),1,1,1))
rng=random.Random(0x422e2c)
for i in range(500):
    matrix=[rng.uniform(-1,1) for _ in range(9)]
    physics=[rng.uniform(-1,1) for _ in range(9)]
    inputs.append(struct.pack('<18f3I',*(matrix+physics),*[rng.randrange(4) for _ in range(3)]))
for payload in inputs:
    u.mem_write(BASE+0x48,payload[:36]);u.mem_write(BASE+0xfc,payload[36:72])
    u.mem_write(BASE+0x858,struct.pack('<I',BASE+0x2000));u.mem_write(BASE+0x2014,payload[72:])
    u.reg_write(UC_X86_REG_ESI,BASE);u.reg_write(UC_X86_REG_EBX,0);u.reg_write(UC_X86_REG_ESP,STACK)
    u.emu_start(0x422e2c,0x422e82,count=100000);assert u.reg_read(UC_X86_REG_EIP)==0x422e82
    expected.append(bytes(4)+bytes(u.mem_read(BASE+0x864,12))+bytes(u.mem_read(BASE+0x87c,12)))
probe=ROOT/'build/pc/Release/rf_eye_probe.exe'
actual=subprocess.check_output([str(probe),'--spawn-angles'],input=b''.join(inputs))
assert len(actual)==28*len(inputs)
pc_exact=0;max_error=0
for i,want in enumerate(expected):
    got=actual[i*28:(i+1)*28];assert got[:4]==bytes(4)
    assert got==want,('PC',i,got.hex(),want.hex())
    pc_exact+=got==want
    for a,b in zip(struct.unpack('<6f',got[4:]),struct.unpack('<6f',want[4:])):
        max_error=max(max_error,abs(a-b));assert math.isfinite(a) and abs(a-b)<=2e-7*max(1,abs(b)),(i,a,b)
nxdk=False
if '--nxdk' in sys.argv:
    x=machine(ROOT/'build/xbox/main.exe')
    address=int(re.search(r'_rf_look_spawn_angles\s+([0-9a-fA-F]+)',(ROOT/'build/xbox/main.map').read_text())[1],16)
    for i,payload in enumerate(inputs):
        x.mem_write(BASE,payload);x.mem_write(STACK,struct.pack('<5I',STOP,BASE,BASE+36,BASE+72,BASE+0x100))
        x.reg_write(UC_X86_REG_FPCW,0x27f)
        x.reg_write(UC_X86_REG_ESP,STACK);x.emu_start(address,STOP,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==STOP
        assert x.reg_read(UC_X86_REG_FPCW)==0x27f
        got=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(BASE+0x100,24))
        assert got==expected[i],('NXDK',i,got.hex(),expected[i].hex())
    nxdk=True
report=dict(result='PASS',original_sha256=sha,cases=len(inputs),installed_levels=len(starts),pc_bit_exact=pc_exact,
    pc_probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
    nxdk_sha256=hashlib.sha256((ROOT/'build/xbox/main.exe').read_bytes()).hexdigest() if nxdk else None,
    pc_max_absolute_error=max_error,nxdk_bit_exact=nxdk,
    scope='Original 422e2c..422e82 with all callees, supplied body/physics matrices and rotation references. 94 installed starts, seven axis cases and 500 synthetic matrices; no live factory integration.')
(ROOT/'artifacts/spawn-look-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
