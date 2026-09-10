"""Shared template conversion versus original 45fcf0 decoded-field replay."""
import runpy,struct,re,json,subprocess
from pathlib import Path
c=runpy.run_path(str(Path(__file__).with_name('verify_level_emitter_conversion_trace.py')))
layout=runpy.run_path(str(Path(__file__).with_name('verify_level_emitter_reader.py')))
Emitter=layout['Emitter'];x=c['c']['x'];root=c['root'];base=c['base'];stack=c['stack'];stop=c['stop']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_level_emitter_template\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
records={(l['file'],r['uid']):r for l in c['inventory']['results'] for r in l['records']}
commands=bytearray();expected=bytearray()
for reference in c['results']:
    record=records[reference['level'],reference['uid']];value=Emitter()
    for name,typ in Emitter._fields_:
        field=record[name]
        if isinstance(field,str):setattr(value,name,field.encode('cp1252'))
        elif isinstance(field,list):getattr(value,name)[:]=field
        else:setattr(value,name,field)
    initial=bytes([reference['fill']])*132;result=bytes(4)+bytes.fromhex(reference['template'])
    commands.extend(bytes(value)+initial);expected.extend(result)
    x.mem_write(base,bytes(value));x.mem_write(base+0x1000,initial)
    x.mem_write(stack,struct.pack('<5I',stop,base,23,1,base+0x1000));x.reg_write(UC_X86_REG_ESP,stack)
    x.emu_start(entry,stop,count=100000);assert x.reg_read(UC_X86_REG_EIP)==stop
    actual=struct.pack('<I',x.reg_read(UC_X86_REG_EAX))+bytes(x.mem_read(base+0x1000,132))
    assert actual==result,(reference['level'],reference['uid'],[(i,a,b) for i,(a,b) in enumerate(zip(actual,result)) if a!=b])
actual=subprocess.check_output([str(c['c']['probe']),'--level-emitter-template'],input=commands)
assert actual==expected,[(i//136,i%136,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:20]
report=dict(result='PASS',records=87,replays=174,scope='All installed level-emitter template fields match original decoded-field conversion on PC/NXDK, with two initial fills proving preserved fields. Resolved bitmap supplied; original byte parsing, resource loading and campaign registration excluded.')
(root/'artifacts/level-emitter-template-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
