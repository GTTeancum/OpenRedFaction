"""Full original506430 versus PC/NXDK, no substituted functions."""
import hashlib,json,random,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
b=0x30000000;stack=b+0xe000;stop=b+0xf000
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
f=lambda v:struct.pack('<'+'f'*len(v),*v)
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def machine(path):
    p=pefile.PE(str(path));d=p.get_memory_mapped_image();o=p.OPTIONAL_HEADER.ImageBase
    m=Uc(UC_ARCH_X86,UC_MODE_32);m.mem_map(o,(len(d)+4095)//4096*4096);m.mem_write(o,d);m.mem_map(b,65536);m.reg_write(UC_X86_REG_FPCW,0x27f);return m
u=machine(exe);x=machine(root/'build/xbox/main.exe');mapping=(root/'build/xbox/main.map').read_text()
entry=int(re.search(r'\s_rf_collision_model_ray_plane\s+([0-9a-fA-F]+)',mapping)[1],16)
rng=random.Random(0x506430);commands=[];answers=[];hits=0;changed_miss=0;parallel=0
for case in range(8192):
    start=[rng.uniform(-100,100) for _ in range(3)];delta=[rng.uniform(-100,100) for _ in range(3)]
    plane=[rng.uniform(-1,1) for _ in range(3)]+[rng.uniform(-20,20)]
    if case<4096:
        axis=case%3;plane=[0.,0.,0.,0.];plane[axis]=(-1.,1.)[case%2]
        start[axis]=(-2.,-1.,-0.,0.,1.,2.)[case//6%6]
        delta[axis]=(-4.,-2.,-1.,-0.,0.,1.,2.,4.)[case//36%8]
        plane[3]=(-1.,0.,1.)[case//288%3]
    if 4096<=case<4352:
        # Finite subnormal denominators, including float-store underflow.
        plane=[struct.unpack('<f',w(1+(case%32)))[0],0,0,0]
        start=[(-1.,0.,1.)[case%3],0,0];delta=[(.25,1.,-1.,2.)[case%4],0,0]
    seed=f([11,12,13,14]);command=f(start+delta+plane)+seed;commands.append(command)
    outputs=[]
    for m,native in ((u,False),(x,True)):
        m.mem_write(b,command+b'\xa5'*16)
        args=(b,b+12,b+24,b+40) if native else (b+40,b,b+12,b+24)
        m.mem_write(stack,w(stop,*args));m.reg_write(UC_X86_REG_ESP,stack)
        m.emu_start(entry if native else 0x506430,stop,count=10000)
        assert m.reg_read(UC_X86_REG_EIP)==stop and m.reg_read(UC_X86_REG_ESP)==stack+4
        assert bytes(m.mem_read(b,40))==command[:40] and bytes(m.mem_read(b+56,16))==b'\xa5'*16
        outputs.append(w(m.reg_read(UC_X86_REG_EAX)&255)+bytes(m.mem_read(b+40,16)))
    assert outputs[0]==outputs[1],('NXDK',case,command.hex(),outputs)
    answers.append(outputs[0]);accepted=struct.unpack('<I',outputs[0][:4])[0];hits+=accepted
    if not accepted:
        changed_miss+=outputs[0][4:]!=seed;parallel+=outputs[0][4:]==seed
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--model-ray-plane'],input=b''.join(commands))
assert actual==b''.join(answers),'PC mismatch'
report=dict(result='PASS',cases=len(commands),hits=hits,misses_writing_fraction=changed_miss,misses_preserving_result=parallel,original_sha256=sha,
    scope='Full506430 and real dot/distance/vector callees, no hooks. Exact PC/NXDK point/fraction/low-byte return, preserved inputs and guards under027f. Both directions, endpoints, parallel/coplanar, outside segment, nonunit oblique planes and finite subnormal denominators. No triangle containment or live geometry integration.')
(root/'artifacts/model-ray-plane.json').write_text(json.dumps(report,indent=2));print(report)
