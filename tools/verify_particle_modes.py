"""Verify particle mode initialization/selection and the actual texture-source-2 pass."""
import runpy,struct,re,random,subprocess,json,itertools
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_render_states.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_EAX
mapping=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
def original(entry):
    u.mem_write(stack,struct.pack('<I',stop));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(entry,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
original(0x50be10);original(0x50be40)
normal,glow=(struct.unpack('<I',u.mem_read(a,4))[0] for a in (0x17c7c58,0x1775b30))
assert (normal,glow)==(0x118c42,0x6110c42)
rng=random.Random(49490);commands=bytearray();results=bytearray();mode_entry=symbol('rf_particle_render_mode')
# Include unrelated flag bits and arbitrary supplied modes to check that the
# no-Z override preserves every bit outside the five-bit depth field.
fixtures=[(f,normal,glow) for f in (0,2,0x2000,0x2002,0xffffffff,0x4000,0x4002)]
fixtures += [(rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)) for _ in range(1024)]
for flags,n,g in fixtures:
    u.mem_write(0x17c7c58,struct.pack('<I',n));u.mem_write(0x1775b30,struct.pack('<I',g))
    u.mem_write(base+0x1058,struct.pack('<I',flags));u.reg_write(UC_X86_REG_ESI,base+0x1000)
    u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x494c8f,0x494cbc,count=1000);assert u.reg_read(UC_X86_REG_EIP)==0x494cbc
    result=bytes(u.mem_read(stack+8,4));results.extend(result)
    commands.extend(struct.pack('<III',flags,n,g))
    x.mem_write(stack,struct.pack('<4I',stop,flags,n,g));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(mode_entry,stop,count=1000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))==result
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--particle-render-mode'],input=commands)==results
texture_entry=symbol('rf_particle_texture_decode');commands.clear();results.clear();texture_cases=0
for color,alpha,lod in itertools.product(range(32),range(32),(0,0x3f800000,0xbf800000,0x7fc12345)):
    mode=2 | color<<5 | alpha<<10 | (glow & ~0x7fff)
    c['word'](0x1e64da0,0xffffffff);c['word'](0x5aa7f0,lod)
    c['render_calls'].clear();c['texture_calls'].clear()
    u.mem_write(stack,struct.pack('<II',stop,mode));u.reg_write(UC_X86_REG_ESP,stack)
    u.emu_start(0x54f160,stop,count=10000);assert u.reg_read(UC_X86_REG_EIP)==stop
    calls=c['texture_calls'];assert len(calls)==12
    result=struct.pack('<II',0,len(calls))+b''.join(struct.pack('<III',*v) for v in calls)
    commands.extend(struct.pack('<II',mode,lod));results.extend(result)
    x.mem_write(base,bytes([0xa5])*148)
    x.mem_write(stack,struct.pack('<4I',stop,mode,lod,base));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(texture_entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,148))==result,(color,alpha,lod)
    texture_cases+=1
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--particle-texture-states'],input=commands)==results
commands.clear();results.clear()
for texture in (t for t in range(32) if t!=2):
    mode=(normal&~31)|texture
    commands.extend(struct.pack('<II',mode,0))
    result=struct.pack('<i',-3)+bytes([0xa5])*148;results.extend(result)
    x.mem_write(base,bytes([0xa5])*148)
    x.mem_write(stack,struct.pack('<4I',stop,mode,0,base));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(texture_entry,stop,count=10000);assert x.reg_read(UC_X86_REG_EIP)==stop
    assert struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base,148))==result
assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--particle-texture-states'],input=commands)==results
report=dict(result='PASS',initializers={'50be10':hex(normal),'50be40':hex(glow)},selection_cases=len(fixtures),texture_cases=texture_cases,unsupported_texture_guards=31,
    scope='Original constructors and selection span, including actual 496a30 override; full original 54f160 texture-source 2 versus PC/NXDK ordered texture-stage calls. No GPU rasterization or texture binding tested.')
(root/'artifacts/particle-modes-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
