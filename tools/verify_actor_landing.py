"""Execute prepared original static-support and ordinary run-landing blocks."""
import json, struct, subprocess, sys, hashlib, re
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_FPCW,UC_X86_REG_EAX
exe=root/'Installed_Game/RF.exe'
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image=pefile.PE(str(exe)).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x400000,(len(image)+4095)//4096*4096);u.mem_write(0x400000,image)
base=0x30000000;stack=base+0xe000;stop=base+0xf000;u.mem_map(base,0x10000)
w=lambda *v:struct.pack('<%dI'%len(v),*v)
f=lambda *v:struct.pack('<%df'%len(v),*v)
d=json.loads(Path(sys.argv[1]).read_text());records=d['symbols']['rf_scene_actor_ground_records']['words']
commands=[];expected=[];support_expected=[];frames=[]
for frame in range(64):
    record=records[frame*33:(frame+1)*33]
    if not record[32] or struct.unpack('<f',w(record[21]))[0]>=1:continue
    state=bytearray(w(*d['symbols']['rf_scene_actor_initial_state']['words']))
    position=d['symbols']['rf_scene_actor_render_frames']['words'][frame*5+2:frame*5+5]
    state[88:112]=w(*position,*position)
    # Exercise existing X/Z velocity preservation and support/airborne flag clears.
    state[184:196]=f(.25,-3.5,-.125);state[272:276]=w(0x600078)
    command=bytes(state)+w(*record[:21])+w(record[21]);commands.append(command)
    u.mem_write(base,bytes(0x10000));u.mem_write(base+0xf0,w(*position))
    u.mem_write(base+0x144,bytes(state[184:196]));u.mem_write(base+0x180,bytes(state[244:248]));u.mem_write(base+0x1a8,bytes(state[272:276]))
    u.mem_write(base+0x294,w(base+0x3000));u.mem_write(base+0x3724,w(0x0102411f))
    u.mem_write(base+0x3034,w(base+0x4000));u.mem_write(base+0x4000,b'run\0')
    u.mem_write(base+0x75c,w(0xffffffff));u.mem_write(base+0x98,f(100))
    u.mem_write(base+0x3050,f(4));u.mem_write(base+0x305c,f(10))
    # Prepared registered run descriptor, no replacement for lookup callees.
    u.mem_write(0x62fe70,w(1,1,0,0,0,0,0,0))
    u.mem_write(base+0x858,w(0x62fe70))
    u.mem_write(stack+0xc,w(*record[:3]));u.mem_write(stack+0x18,w(*record[3:6]))
    u.mem_write(stack+0x48,bytes(68));u.mem_write(stack+0x60,w(record[21]));u.mem_write(stack+0x78,w(0xffffffff))
    u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_EBX,base+0xf0)
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_FPCW,0x37f)
    u.emu_start(0x4a0b31,0x4a0bfa,count=10000);assert u.reg_read(UC_X86_REG_EIP)==0x4a0bfa
    supported=bytearray(command[:308])
    for dst,src,n in [(88,0xe4,12),(100,0xf0,12),(184,0x144,12),(248,0x190,24),(272,0x1a8,4)]:supported[dst:dst+n]=bytes(u.mem_read(base+src,n))
    support_expected.append(bytes(supported))
    u.mem_write(stack,w(stop));u.reg_write(UC_X86_REG_ESP,stack-0x2c)
    # Enter after sound/effect handling; execute velocity and descriptor changes.
    u.emu_start(0x419901,stop,count=100000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert bytes(u.mem_read(base+0x858,4))==w(0x62fe70)
    for dst,src,n in [(88,0xe4,12),(100,0xf0,12),(184,0x144,12),(248,0x190,24),(272,0x1a8,4)]:state[dst:dst+n]=bytes(u.mem_read(base+src,n))
    expected.append(bytes(state));frames.append(frame)
    # Current passive fixture reaches support before a bounce: no X/Z speed,
    # force or steering. Verify its idle proposal through original dispatch.
    u.mem_write(base+0x144,f(0,0,0));u.mem_write(base+0x168,f(0,0,0))
    u.mem_write(base+0x714,f(0,0,0));u.mem_write(base+0x1b0,f(1/60))
    u.mem_write(base+0x1a8,w(0x78));u.mem_write(base+0x8c,f(10))
    u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ESI,base)
    u.emu_start(0x49f646,0x49f8aa,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x49f8aa
    assert bytes(u.mem_read(base+0xf0,12))==bytes(u.mem_read(base+0xe4,12))
    assert bytes(u.mem_read(base+0x144,12))==f(0,0,0)
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--land'],input=b''.join(commands))
assert actual==b''.join(expected),'Shared landing differs from original prepared blocks'
actual=subprocess.check_output([str(root/'build/pc/Release/rf_physics_probe.exe'),'--support'],input=b''.join(commands))
assert actual==b''.join(support_expected),'Shared support commit differs from original'
xp=pefile.PE(str(root/'build/xbox/main.exe'));xb=xp.get_memory_mapped_image();origin=xp.OPTIONAL_HEADER.ImageBase
x=Uc(UC_ARCH_X86,UC_MODE_32);x.mem_map(origin,(len(xb)+4095)//4096*4096);x.mem_write(origin,xb);x.mem_map(base,0x10000)
for name,outputs in [('land',expected),('support',support_expected)]:
    entry=int(re.search(r'_rf_physics_static_'+name+r'\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
    for command,wanted in zip(commands,outputs):
        x.mem_write(base,command[:308]);x.mem_write(base+0x2000,command[308:392])
        x.mem_write(stack,w(stop,base,base+0x2000)+command[392:396])
        x.reg_write(UC_X86_REG_ESP,stack);x.reg_write(UC_X86_REG_FPCW,0x27f)
        x.emu_start(entry,stop,count=100000)
        assert x.reg_read(UC_X86_REG_EIP)==stop and x.reg_read(UC_X86_REG_EAX)==0,(hex(x.reg_read(UC_X86_REG_EIP)),x.reg_read(UC_X86_REG_EAX))
        assert bytes(x.mem_read(base,308))==wanted,('NXDK differs from original',name)
for fraction,normal in [(1.,(0,1,0)),(.5,(1,0,0))]:
    u.mem_write(base+0x858,w(0x62fe70));u.mem_write(base+0x1a8,w(0x78))
    u.mem_write(0x62feb0,w(1,3,0,0,0,0,0,0));u.mem_write(0x7c6ec8,f(0,1,0))
    u.mem_write(stack-0x10,bytes(0xb0));u.mem_write(stack+0x60,f(fraction));u.mem_write(stack+0x54,f(*normal))
    u.mem_write(stack+0x9c,w(stop));u.reg_write(UC_X86_REG_ESI,base);u.reg_write(UC_X86_REG_ESP,stack-0x10)
    u.emu_start(0x4a0a5c,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    assert bytes(u.mem_read(base+0x858,4))==w(0x62feb0) and bytes(u.mem_read(base+0x1a8,4))==w(0x79)
report=dict(status='PASS',cases=len(frames),nxdk_cases=len(frames),support_commit_cases=len(frames),original_idle_cases=len(frames),original_support_loss_cases=2,frames=frames,scope='4a0b31..4a0bfa static support position/bounds; 419901..return ordinary run landing with actual callees and prepared descriptor. Original 49f646..49f8aa zero-input idle and 4a0a5c no-hit/steep support loss also checked. No damage/sound/AI state, zero support/contact velocity. Synthetic nonzero X/Z input retained in support and landing comparison.')
(root/'artifacts/actor-landing-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
