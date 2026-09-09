"""Run original skinning preparation with its pose generations already current."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;u.mem_map(base,131072);definition=base+4096;instance=base+16384;esp=base+100000;stop=base+120000
rng=random.Random(5100);cases=[];expected=[];refreshes=0
for n in range(2000):
    arrays=[struct.pack('<48f',*(rng.randint(-32,32)/16 for _ in range(48))) for _ in range(3)]
    generation=rng.randrange(65536);stamps=[generation if rng.randrange(2) else (generation-1)&65535 for _ in range(4)]
    refreshes+=sum(s!=generation for s in stamps)
    cases.append(b''.join(arrays)+struct.pack('<6H',*stamps,generation,0))
    u.mem_write(definition,bytes(8192));u.mem_write(instance,bytes(8192))
    u.mem_write(definition+0x48,struct.pack('<I',4));u.mem_write(definition+0xf24,bytes(range(4)))
    u.mem_write(instance+0x1d50,struct.pack('<I',definition));u.mem_write(instance+0x1cf8,struct.pack('<H',generation))
    u.mem_write(instance,arrays[1]);u.mem_write(instance+0x960,arrays[2])
    for i in range(4):
        u.mem_write(definition+0x64+i*76,arrays[0][i*48:i*48+48])
        u.mem_write(instance+0x1394+i*48,struct.pack('<2H',generation,stamps[i]))
    u.mem_write(esp,struct.pack('<7I',stop,0,0,0,0,0,0));u.reg_write(UC_X86_REG_ESP,esp);u.reg_write(UC_X86_REG_ECX,instance)
    u.emu_start(0x51ba00,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected.append(bytes(u.mem_read(instance+0x960,192))+b''.join(bytes(u.mem_read(instance+0x1396+i*48,2)) for i in range(4)))
actual=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--prepare-skinning'],input=b''.join(cases))
assert actual==b''.join(expected)
report=dict(result='PASS',cases=len(cases),bones=len(cases)*4,refreshes=refreshes,
    scope='Complete unchanged 0x51ba00 and callees with pose generations pre-current; stale/current prepared stamps and exact dyadic matrix outputs; pose recomputation and original error behavior excluded')
(root/'artifacts/prepare-skinning-verification.json').write_text(json.dumps(report,indent=2));print(report)
