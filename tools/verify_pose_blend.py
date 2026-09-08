"""Compare complete local pose blending with unhooked original 0x51b110."""
import hashlib,json,struct,subprocess,sys,random,math
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image(); u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096); u.mem_write(0x400000,image)
data,stack,stop=0x30000000,0x31000000,0x32000000
for a in (data,stack,stop): u.mem_map(a,65536)
rng=random.Random(0x51b110); inputs=[]
for trial in range(1600):
    count=trial%16+1; q=[]
    for i in range(count):
        v=[rng.uniform(-1,1) for _ in range(4)]; length=math.sqrt(sum(x*x for x in v)); q.extend(x/length for x in v)
    p=[rng.uniform(-20,20) for _ in range(count*3)]
    w=[rng.uniform(.01,1) for _ in range(count)]; total=sum(w); w=[x/total for x in w]
    inputs.append(struct.pack('<I',count)+struct.pack('<'+'f'*(count*8),*(q+p+w)))
run=subprocess.run([str(root/'build/pc/Release/rf_pose_probe.exe')],input=b''.join(inputs),capture_output=True,check=True)
assert len(run.stdout)==len(inputs)*52
failures=[]
for i,raw in enumerate(inputs):
    n,=struct.unpack_from('<I',raw); u.mem_write(data,raw[4:]); out=data+4096
    u.mem_write(stack+64000,struct.pack('<6I',stop,out,data,data+n*16,n,data+n*28))
    u.reg_write(UC_X86_REG_ESP,stack+64000); u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x51b110,stop,count=50000)
    assert u.reg_read(UC_X86_REG_EIP)==stop
    want=bytes(u.mem_read(out,48)); status,=struct.unpack_from('<i',run.stdout,i*52); got=run.stdout[i*52+4:(i+1)*52]
    if status or got!=want: failures.append(dict(index=i,count=n,status=status,expected=want.hex(),actual=got.hex()))
report=dict(result='PASS' if not failures else 'FAIL',samples=len(inputs),failures=len(failures),examples=failures[:10])
(root/'artifacts/pose-blend-verification.json').write_text(json.dumps(report,indent=2)); print(report)
assert not failures
