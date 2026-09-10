"""Shared emitter initializer against full unchanged original 497020 fixtures."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_emitter_init_trace.py')))
x=c['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_emitter_initialize\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000);storage=base+0x10000;lists=base+0x8000;pool=base+0x8100
def compact(raw):
    return raw[4:0x9c]+raw[0x154:0x158]+raw[0x128:0x138]+raw[0x140:0x144]+raw[0x13c:0x140]+raw[0x138:0x13c]
commands=bytearray();expected=bytearray()
for f in c['fixtures']:
    before=compact(bytes.fromhex(f['before']));after=compact(bytes.fromhex(f['after']));source=bytes.fromhex(f['source'])
    created=bool(f['flags']&0x10 and f['flags']&2 and not f['empty'])
    particle=bytearray.fromhex(f['particle']);particle[:8]=struct.pack('<2I',1605,1605) if created else struct.pack('<2I',501,1601)
    if created:particle[0x68:0x6c]=struct.pack('<I',1)
    init_result=struct.pack('<4I',created,500 if created else 0xffffffff,struct.unpack_from('<I',source)[0],struct.unpack_from('<I',source,128)[0])
    result=struct.pack('<II',0,f['rng'])+after+init_result+particle
    commands.extend(before+source+struct.pack('<4I',f['seed'],f['now'],f['empty'],f['enabled']));expected.extend(result)
    x.mem_write(base,before);x.mem_write(base+0x400,source);x.mem_write(base+0x200,struct.pack('<I',f['seed']));x.mem_write(base+0x220,bytes([0xa5])*16)
    x.mem_write(pool,struct.pack('<5I',storage,lists,6,0,0))
    for i in range(6):x.mem_write(lists+i*8,struct.pack('<2I',1600+i,1600+i))
    if not f['empty']:x.mem_write(lists+8,struct.pack('<2I',500,1599))
    x.mem_write(storage+500*120,struct.pack('<2I',501,1601)+bytes([0xa5])*112);x.mem_write(storage+501*120,struct.pack('<2I',502,500))
    x.mem_write(stack,struct.pack('<12I',stop,pool,base,base+0x400,0xffffffff,0x2468,1,f['enabled'],f['now'],0,base+0x200,base+0x220))
    x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,4))+bytes(x.mem_read(base,184))+bytes(x.mem_read(base+0x220,16))+bytes(x.mem_read(storage+500*120,120))
    assert actual==result,(f['case'],[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
    assert struct.unpack('<I',x.mem_read(pool+16,4))[0]==created
    assert struct.unpack('<I',x.mem_read(lists+40,4))[0]==(500 if created else 1605)
actual=subprocess.check_output([str(c['c']['probe']),'--particle-emitter-init'],input=commands)
assert len(actual)==len(expected)
assert actual==expected,[(i//328,i%328,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',cases=1024,scope='Full original 497020 resolved-room parentless path versus PC/NXDK shared initializer. Exact compact runtime, opaque outputs, particle payload and RNG including immediate emission/exhaustion, delay and phase initialization. Room traversal, fresh-object defaults and campaign lifecycle excluded.')
(root/'artifacts/particle-emitter-init-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
