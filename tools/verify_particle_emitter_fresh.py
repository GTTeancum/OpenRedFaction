"""Original static emitter construction establishes first-use compact defaults."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_particle_duration.py')))
u,x,root,base,stack,stop=(c[k] for k in ('u','x','root','base','stack','stop'))
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX,UC_X86_REG_EAX
entry=int(re.search(r'_rf_particle_emitter_fresh\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def call(machine,address,this):
    machine.mem_write(stack,struct.pack('<II',stop,this));machine.reg_write(UC_X86_REG_ESP,stack);machine.reg_write(UC_X86_REG_ECX,this)
    machine.emu_start(address,stop,count=100000);assert machine.reg_read(UC_X86_REG_EIP)==stop
# Prove constructor writes independently of zero-filled image storage.
for fill in (0,0x5a,0xa5,0xff):
    raw=bytearray([fill])*344;u.mem_write(base,bytes(raw));call(u,0x496fd0,base)
    raw[340:344]=bytes([255])*4;assert bytes(u.mem_read(base,344))==raw
# PE zero-initialized backing plus the actual 128-element C++ constructor loop.
assert not any(u.mem_read(0x7b2a70,128*344))
u.mem_map(0,4096) # SEH chain storage used by the unchanged CRT array helper.
call(u,0x496a70,0)
commands=bytearray();expected=bytearray()
for i in range(128):
    raw=bytes(u.mem_read(0x7b2a70+i*344,344));assert raw==bytes(340)+bytes([255])*4
    compact=raw[4:0x9c]+raw[0x154:0x158]+raw[0x128:0x138]+raw[0x140:0x144]+raw[0x13c:0x140]+raw[0x138:0x13c]
    command=bytes([i])*184;commands.extend(command);expected.extend(bytes(4)+compact)
    x.mem_write(base,command);call(x,entry,base)
    assert x.reg_read(UC_X86_REG_EAX)==0 and bytes(x.mem_read(base,184))==compact
assert subprocess.check_output([str(c['probe']),'--particle-emitter-fresh'],input=commands)==expected
report=dict(result='PASS',emitter_capacity=128,original_record_bytes=344,original_pool_bytes=44032,compact_runtime_bytes=184,constructor_poison_cases=4,scope='Original static zero backing, unchanged 496a70/5736fb array construction and 496fd0 members. Only deadline is explicitly written; all other fresh defaults derive from zero storage. PC/NXDK first-use helper matches all 128 compact states. Slot reuse/free-list lifecycle and campaign loading excluded.')
(root/'artifacts/particle-emitter-fresh-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
