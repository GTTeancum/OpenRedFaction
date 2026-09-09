"""Execute original actor response on a stationary-floor fixture from guest RAM."""
import hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile,capstone
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
d=json.loads(Path(sys.argv[1]).read_text());state=d['symbols'].get('actor_trajectory',d['symbols']['scene_actor_body'])['words']
f=lambda *v:struct.pack('<%df'%len(v),*v)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
assert state[55:58]==[0,0,0], 'This passive fixture requires zero applied force'
velocity=list(struct.unpack('<3f',w(*state[46:49])))
increment=struct.unpack('<f',f(struct.unpack('<f',f(-9.8))[0]*struct.unpack('<f',f(1/60))[0]))[0]
for _ in range(d['symbols']['rf_actor_fall_diagnostic']['words'][2]+1):velocity[1]=struct.unpack('<f',f(velocity[1]+increment))[0]
normal=struct.unpack('<3f',w(*d['symbols']['rf_scene_actor_contact']['words'][1:4])) if 'rf_scene_actor_contact' in d['symbols'] else (.1780281662940979,.9819024205207825,-.0646035447716713)
u.mem_write(base,bytes(0x1500));u.mem_write(base+0x144,f(*velocity))
u.mem_write(base+0x1c0,f(*normal))
u.mem_write(base+0x1e4,w(0xffffffff));u.mem_write(base+0x858,w(base+0x2000));u.mem_write(base+0x2004,w(2))
u.mem_write(base+0x1a8,w(state[68]));u.mem_write(base+0x98,f(100));u.mem_write(base+0x294,w(base+0x3000))
u.mem_write(stack,w(stop,base));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);calls=[]
def trace(cpu,address,size,data):
    op=next(decoder.disasm(bytes(cpu.mem_read(address,size)),address),None)
    if op and op.mnemonic=='call':calls.append(dict(address=hex(address),target=op.op_str))
u.hook_add(UC_HOOK_CODE,trace)
report=dict(scope='Synthetic stationary non-liquid contact, falling descriptor mode 2, no support velocity; guest-derived velocity. No function hooks.')
try:
    u.emu_start(0x49d7e0,stop,count=100000)
    report['returned']=u.reg_read(UC_X86_REG_EIP)==stop
except Exception as error:report['error']=str(error)
report.update(eip=hex(u.reg_read(UC_X86_REG_EIP)),calls=calls,velocity_before=velocity,velocity_after=struct.unpack('<3f',u.mem_read(base+0x144,12)))
if report.get('returned'):
    shared=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--contact'],input=f(*velocity,0,0,0,*normal,0,0,0,0,0,0))
    assert shared[:24]==bytes(u.mem_read(base+0x144,24)), 'Shared response differs on guest contact'
    report['pc_matches_original']=True
    if 'rf_scene_actor_contact' in d['symbols']:
        assert shared[:12]==w(*d['symbols']['rf_scene_actor_contact']['words'][4:7]), 'Guest response differs from original'
        report['guest_matches_original']=True
(root/'artifacts/actor-contact-original.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
