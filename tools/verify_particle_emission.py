"""Compare shared PC/NXDK parentless emission with full original reference runs."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_emission_trace.py')))
x=c['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_emitter_emit\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
x.mem_map(base+0x10000,0x30000)
storage=base+0x10000;lists=base+0x8000;pool=base+0x8100
commands=bytearray();expected=bytearray()
for fixture in c['fixtures']:
    before=bytes.fromhex(fixture['before']);after=bytes.fromhex(fixture['after'])
    emitter=before[4:0x9c]+before[0x154:0x158]
    result_emitter=after[4:0x9c]+after[0x154:0x158]
    empty=fixture['empty'];seed=fixture['seed'];now=fixture['now_ms']
    commands.extend(emitter+struct.pack('<3I',seed,now,empty))
    particle=bytearray.fromhex(fixture['particle'])
    particle[:8]=struct.pack('<2I',501,1601) if empty else struct.pack('<2I',1605,1605)
    if not empty:particle[0x68:0x6c]=struct.pack('<I',1)
    result=struct.pack('<iII',-3 if empty else 0,fixture['rng'],0xa5a5a5a5 if empty else 500)+result_emitter+particle
    expected.extend(result)
    x.mem_write(base,emitter);x.mem_write(base+0x200,struct.pack('<II',seed,0xa5a5a5a5))
    x.mem_write(pool,struct.pack('<5I',storage,lists,6,0,0))
    for i in range(6):x.mem_write(lists+i*8,struct.pack('<2I',1600+i,1600+i))
    if not empty:x.mem_write(lists+8,struct.pack('<2I',500,1599))
    x.mem_write(storage+500*120,struct.pack('<2I',501,1601)+bytes([0xa5])*112)
    x.mem_write(storage+501*120,struct.pack('<2I',502,500))
    x.mem_write(stack,struct.pack('<7I',stop,pool,base,1,now,base+0x200,base+0x204))
    x.reg_write(UC_X86_REG_ESP,stack);x.emu_start(entry,stop,count=100000)
    assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x200,8))+bytes(x.mem_read(base,156))+bytes(x.mem_read(storage+500*120,120))
    assert struct.unpack('<I',x.mem_read(pool+16,4))[0]==(0 if empty else 1)
    assert struct.unpack('<I',x.mem_read(lists+40,4))[0]==(1605 if empty else 500)
    assert actual==result,(fixture['case'],[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b][:20])
actual=subprocess.check_output([str(c['c']['probe']),'--particle-emission'],input=commands)
assert len(actual)==len(expected),(len(actual),len(expected))
assert actual==expected,[(i//288,i%288,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',cases=len(c['fixtures']),scope='Full parentless original emission versus PC/NXDK shared C: spawn packet, emitter fields, deadline, RNG, full particle payload with pointer/index translation, successful allocation and exhaustion. Parent resolution and campaign scheduling/rendering excluded.')
(root/'artifacts/particle-emission-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
