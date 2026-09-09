"""Check live stance speed settings against complete original run/slow wrappers."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_FPCW
w=lambda *v:struct.pack('<%dI'%len(v),*v)
f=lambda *v:struct.pack('<%df'%len(v),*v)
base=0x30000000;cls=base+0x3000;stack=base+0xe000;stop=base+0xf000
exe=root/'Installed_Game/RF.exe';assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
def load(path):
    pe=pefile.PE(str(path));b=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
    cpu=Uc(UC_ARCH_X86,UC_MODE_32);cpu.mem_map(origin,(len(b)+4095)//4096*4096);cpu.mem_write(origin,b);cpu.mem_map(base,0x10000);return cpu
u=load(exe);x=load(root/'build/xbox/main.exe')
entry=int(re.search(r'_rf_movement_set_mode\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
s=json.loads(Path(sys.argv[1]).read_text())['symbols'];config=s['rf_scene_actor_movement_config']['words'];frames=s['rf_scene_actor_movement_frames']['words'];body=s['rf_scene_actor_initial_state']['words'];stances=s['rf_scene_actor_stance_frames']['words'];commands=[]
assert config[0]&1 and len(frames)==192
for frame in range(64):
    crouched=bool(stances[frame*4+1]&0x400);mode=0 if crouched else 1
    seed=w(body[1],0,1);want=w(*frames[frame*3:frame*3+3])
    u.mem_write(base,bytes(0x10000));u.mem_write(base+0x294,w(cls));u.mem_write(cls+0x724,w(config[0]));u.mem_write(cls+0x50,w(*config[1:5]))
    u.mem_write(base+0x8c,w(body[1]));u.mem_write(base+0x98,w(body[3]));u.mem_write(base+0x75c,w(0xffffffff));u.mem_write(base+0x148,f(-2))
    u.mem_write(cls+0x34,w(base+0x5000));u.mem_write(base+0x5000,b'run\0');u.mem_write(0x62fe70,w(1,1,2,0,2,1,3,0))
    u.mem_write(0x64ecb9,b'\0');u.mem_write(stack,w(stop,base,1));u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x428030 if crouched else 0x4280b0,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    actual=bytes(u.mem_read(base+0x8c,4))+bytes(u.mem_read(base+0x8c0,8))
    assert actual==want,('Original differs',frame,actual.hex(),want.hex())
    assert bytes(u.mem_read(base+0x858,8))==w(0x62fe70,0x73a858) and bytes(u.mem_read(base+0x148,4))==w(0)
    commands.append(seed+w(*config)+w(mode,0xffffffff,body[3],0))
    x.mem_write(base,seed);x.mem_write(cls,w(*config));x.mem_write(stack,w(stop,base,cls,mode,0xffffffff,body[3],0));x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0
    assert bytes(x.mem_read(base,12))==want
pc=subprocess.check_output([str(root/'build/pc/Release/rf_movement_probe.exe')],input=b''.join(commands))
assert pc==b''.join(w(0,*frames[i:i+3]) for i in range(0,192,3))
report=dict(status='PASS',frames=64,original_wrappers=True,pc_nxdk_guest_match=True,scope='Full 428030 forced-crouch setup and 4280b0 already-standing setup with original callees; loaded run descriptor, authored captured fields, no forced action or network override. Overhead query and input-to-target-speed controller are outside this comparison.')
(root/'artifacts/actor-speed-mode-verification.json').write_text(json.dumps(report,indent=2));print(report)
