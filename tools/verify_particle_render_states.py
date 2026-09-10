"""Execute RF 54f160 unchanged and verify render-state calls at a fake D3D device.

Texture-stage calls are recorded but not modeled here. No GPU or host input.
"""
import hashlib, itertools, json, struct, sys, re, subprocess
from pathlib import Path
import pefile
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
exe = root / 'Installed_Game/RF.exe'
sha = hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p = pefile.PE(str(exe))
blob = p.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(p.OPTIONAL_HEADER.ImageBase, (len(blob)+4095)//4096*4096)
u.mem_write(p.OPTIONAL_HEADER.ImageBase, blob)
base = 0x30000000
u.mem_map(base, 65536)
stack, stop, device, vtable = base+0xe000, base+0xf000, base, base+0x100
render_stub, texture_stub = base+0xf010, base+0xf020
def word(address, value):
    u.mem_write(address, struct.pack('<I', value))
word(device, vtable)
word(vtable+0xc8, render_stub)
word(vtable+0xfc, texture_stub)
word(0x1cfcbe4, device)
u.mem_write(0x1e652ed, b'\0')  # Empty batch: no submission to an actual GPU.
u.mem_write(0x1cfcc1d, b'\0')
render_calls, texture_calls = [], []
def hook(m, address, size, data):
    if address not in (render_stub, texture_stub):
        return
    sp = m.reg_read(UC_X86_REG_ESP)
    argc = 3 if address == render_stub else 4
    args = struct.unpack('<'+'I'*(argc+1), m.mem_read(sp, 4*(argc+1)))
    assert args[1] == device
    (render_calls if argc == 3 else texture_calls).append(args[2:])
    m.reg_write(UC_X86_REG_EAX, 0)
    m.reg_write(UC_X86_REG_ESP, sp+4*(argc+1))
    m.reg_write(UC_X86_REG_EIP, args[0])
u.hook_add(UC_HOOK_CODE, hook)
def expected(blend, depth, fog, caps, zmode, fog_on):
    calls = []
    if blend == 0:
        calls += [(27,0)]
    elif blend in (1,2,3,4,5,6,7) and (blend != 5 or caps & 256):
        factors = {1:(2,2),2:(5,2),3:(5,6),4:(5,2),5:(9,1),6:(10,1),7:(9,3)}
        calls += [(27,1)]
        if blend == 3 and not caps & 16:
            calls += [(19,12)]
        else:
            src,dst = factors[blend]
            calls += [(19,src),(20,dst)]
    z = 1 if zmode == 0 else 2 if zmode == 1 else 0
    compare = 7 if zmode == 0 else 4
    if depth == 0:
        calls += [(15,0),(7,0),(14,0)]
    elif depth in (1,2,4):
        calls += [(15,0),(7,z),(14,int(depth==4)),(23,3 if depth==2 else compare)]
    elif depth == 3:
        calls += [(15,0),(7,0),(14,1)]
    elif depth == 5:
        calls += [(7,z),(14,1),(23,compare),(15,1),(24,16),(25,7)]
    if fog <= 3:
        calls += [(28,int(fog < 3 and fog_on != 0))]
    return calls
xpe = pefile.PE(str(root/'build/xbox/main.exe'))
xb = xpe.get_memory_mapped_image()
x = Uc(UC_ARCH_X86, UC_MODE_32)
x.mem_map(xpe.OPTIONAL_HEADER.ImageBase, (len(xb)+4095)//4096*4096)
x.mem_write(xpe.OPTIONAL_HEADER.ImageBase, xb)
x.mem_map(base,65536)
entry = int(re.search(r'_rf_particle_render_decode\s+([0-9a-fA-F]+)', (root/'build/xbox/main.map').read_text())[1],16)
commands, results = bytearray(), bytearray()
cases = 0
for blend, depth, fog, zmode, caps in itertools.product(range(9),range(7),range(5),range(3),(0,16,256,272)):
    fog_on = cases % 2
    fog_kind = (cases // 2) % 3
    color = cases % 6
    alpha = (cases // 6) % 5
    mode = 1 | color<<5 | alpha<<10 | blend<<15 | depth<<20 | fog<<25
    word(0x1e64da0, 0xffffffff)
    word(0x1cfcaf4, caps)
    word(0x17c7c4c, zmode)
    word(0x5a7df8, fog_kind)
    u.mem_write(0x17c7c20, bytes([fog_on]))
    u.mem_write(0x1e652ec, b'\xa5')
    u.mem_write(0x5aa7e4, b'\xa5\x5a')
    render_calls.clear(); texture_calls.clear()
    u.mem_write(stack, struct.pack('<II', stop, mode))
    u.reg_write(UC_X86_REG_ESP, stack)
    u.emu_start(0x54f160, stop, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    assert render_calls == expected(blend,depth,fog,caps,zmode,fog_on), (mode,caps,zmode,render_calls)
    assert bytes(u.mem_read(0x5aa7e4,2)) == bytes([0 if color==1 else 1 if color<5 else 0xa5, 0 if alpha==2 else 1 if alpha<4 else 0x5a])
    assert u.mem_read(0x1e652ec,1)[0] == (int(fog_on and fog_kind==2) if fog<3 else 0 if fog==3 else 0xa5)
    initial = struct.pack('<I',0xa5a5a5a5)+bytes([0xa5])*80+struct.pack('<III',0xa5,0x5a,0xa5)
    command = struct.pack('<5I',mode,caps,zmode,fog_on,fog_kind)+initial
    result = struct.pack('<II',0,len(render_calls))
    result += b''.join(struct.pack('<II',*pair) for pair in render_calls)
    result += bytes([0xa5])*(80-len(render_calls)*8)
    result += struct.pack('<III',*u.mem_read(0x5aa7e4,2),u.mem_read(0x1e652ec,1)[0])
    commands.extend(command); results.extend(result)
    x.mem_write(base,command)
    x.mem_write(stack,struct.pack('<4I',stop,mode,base+4,base+20))
    x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=10000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+20,96))==result, (mode,caps,zmode)
    # Same packed mode is cached: even environment changes cause no state calls.
    render_calls.clear(); texture_calls.clear()
    word(0x1cfcaf4, caps ^ 272)
    u.reg_write(UC_X86_REG_ESP, stack)
    u.emu_start(0x54f160, stop, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    assert not render_calls and not texture_calls
    cases += 1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--particle-render-states'],input=commands)==results
report = dict(result='PASS', cases=cases, cached_repeats=cases, original_sha256=sha,
    scope='Unchanged full 54f160, texture mode 1, empty batch, fake COM device. Exact ordered render-state writes and color/alpha/fog globals versus PC/NXDK C. Original cached repeats checked separately. Texture-stage semantics, particle default modes and GPU output remain unverified.')
(root/'artifacts/particle-render-states-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
