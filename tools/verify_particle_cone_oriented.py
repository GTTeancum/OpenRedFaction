"""Full original 4fae00 oriented cone versus PC and NXDK, including RNG."""
import runpy, struct, re, random, json, subprocess
from pathlib import Path
c = runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u, x, root, base, stack, stop, thread = (c[k] for k in ('u','x','root','base','stack','stop','thread'))
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ECX, UC_X86_REG_EAX
entry = int(re.search(r'_rf_particle_cone_oriented\s+([0-9a-fA-F]+)', (root/'build/xbox/main.map').read_text())[1], 16)
boundary = struct.unpack('<I', struct.pack('<f', 0.0001))[0]
edges = [struct.unpack('<f', struct.pack('<I', boundary + delta))[0] for delta in (-1,0,1)]
axes = [(0,0,0),(0,1,0),(0,-1,0),(0,20,0),(1,0,0),(0,0,1),(1,2,3)]
for edge in edges:
    for sign in (-1,1):
        for y in (-2,0,2):
            axes.extend(((sign*edge,y,0),(0,y,sign*edge),(sign*edge,y,sign*edge)))
rng = random.Random(0x4fae00)
commands = bytearray(); expected = bytearray()

def check_xbox(command, result, case):
    x.mem_write(base,command); x.mem_write(base+32,bytes([0xa5])*12)
    cosine = struct.unpack_from('<f',command,12)[0]
    x.mem_write(stack,struct.pack('<IIfII',stop,base,cosine,base+16,base+32))
    x.reg_write(UC_X86_REG_ESP,stack); x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual = struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+16,4))+bytes(x.mem_read(base+32,12))
    assert actual==result,(case,command.hex(),actual.hex(),result.hex())

for case in range(4096):
    axis = axes[case//6] if case<len(axes)*6 else tuple(rng.randint(-32768,32768)/4096 for _ in range(3))
    cosine = (-1,-0.5,0,0.5,0.99999994,1)[case%6] if case<len(axes)*6 else rng.randint(-32768,32768)/32768
    seed = rng.getrandbits(32); command = struct.pack('<4fI',*axis,cosine,seed)
    commands.extend(command)
    u.mem_write(thread+20,struct.pack('<I',seed)); u.mem_write(base,bytes([0xa5])*12)
    u.mem_write(stack,struct.pack('<I',stop)+command[:16])
    u.reg_write(UC_X86_REG_ESP,stack); u.reg_write(UC_X86_REG_ECX,base)
    u.emu_start(0x4fae00,stop,count=100000); assert u.reg_read(UC_X86_REG_EIP)==stop
    next_seed = (seed*214013+2531011)&0xffffffff
    next_seed = (next_seed*214013+2531011)&0xffffffff
    assert bytes(u.mem_read(thread+20,4))==struct.pack('<I',next_seed)
    result = bytes(4)+struct.pack('<I',next_seed)+bytes(u.mem_read(base,12))
    expected.extend(result); check_xbox(command,result,case)
guards = [(1,2,3,v) for v in (-2,2,float('nan'),float('inf'))]
for component in range(3):
    for bad in (float('nan'),float('inf'),-float('inf')):
        values = [1,2,3,0.5]; values[component] = bad; guards.append(values)
for values in guards:
    command = struct.pack('<4fI',*values,seed)
    result = struct.pack('<iI',-4,seed)+bytes([0xa5])*12
    commands.extend(command); expected.extend(result); check_xbox(command,result,values)
actual = subprocess.check_output([str(c['probe']),'--particle-cone-oriented'],input=commands)
assert len(actual)==len(expected)
assert actual==expected,[(i//20,i%20,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report = dict(result='PASS',cases=4096,invalid_guards=len(guards),original_sha256=c['sha'],scope='Full original 4fae00 basis construction, local cone and rotation unchanged; actual CRT RNG and x87 math. Exact XYZ and two-draw RNG state versus PC/NXDK, including vertical threshold neighbors, zero and nonunit axes. Parent resolution and live emitter execution excluded.')
(root/'artifacts/particle-cone-oriented-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(report)
