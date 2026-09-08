"""Compare complete original material-count query across all wrapper kinds."""
import hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
obj,stack=0x30000000,0x30100000
u.mem_map(obj,65536);u.mem_map(stack,65536)
rng=random.Random(0x503690);cases=[]
for k in range(2000):
    counts=[rng.choice([-2147483648,-1,0,1,17,2147483647]) for _ in range(8)]
    cases.append(struct.pack('<13iI',rng.choice([-1,0,1,2,3,4]),rng.choice([-1,0,1,2]),rng.choice([-1,0,1,100]),rng.randrange(-2,9),rng.choice([-1,0,1,200]),*counts,8))
out=subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--materials'],input=b''.join(cases));assert len(out)==8*len(cases)
def put(a,fmt,*v):u.mem_write(a,struct.pack(fmt,*v))
coverage={}
for k,wire in enumerate(cases):
    kind,lods,static_count,n,direct,*counts=struct.unpack('<13iI',wire)
    put(obj,'<iII',kind,obj+0x100,obj+0x1000)
    put(obj+0x150,'<iI',lods,obj+0x200);put(obj+0x204,'<I',obj+0x300);put(obj+0x384,'<i',static_count)
    put(obj+0x1088,'<i',direct);put(obj+0x29bc,'<i',n)
    for i in range(8):
        put(obj+0x29c0+i*0x94+0x90,'<I',obj+0x4000+i*0x100)
        put(obj+0x4084+i*0x100,'<iI',counts[i],0)
    before=bytes(u.mem_read(obj,65536));stop=stack+65000
    put(stack+64000,'<II',stop,obj);u.reg_write(UC_X86_REG_ESP,stack+64000)
    u.emu_start(0x503690,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    expected=struct.pack('<II',0,u.reg_read(UC_X86_REG_EAX))
    assert out[k*8:k*8+8]==expected,(k,wire.hex(),out[k*8:k*8+8].hex(),expected.hex())
    assert before==bytes(u.mem_read(obj,65536))
    coverage[kind]=coverage.get(kind,0)+1
bad=struct.pack('<13iI',2,0,0,2,0,*([1]*8),1)
assert subprocess.check_output([str(root/'build/pc/Release/rf_model_probe.exe'),'--materials'],input=bad)==struct.pack('<ii',-4,-99)
report=dict(result='PASS',cases=len(cases),kind_coverage=coverage,bounds_rejections=1,scope='Complete unchanged 503690 and 4a76f0; static, animated submesh sum, direct and unknown kinds, signed counts/wrap; query only, no allocation')
(root/'artifacts/model-material-count-verification.json').write_text(json.dumps(report,indent=2));print(report)
