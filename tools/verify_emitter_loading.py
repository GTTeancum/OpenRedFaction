"""Named emitter ownership and archive-budget checks; port binding, not original lookup equivalence."""
import runpy
from pathlib import Path
import re,struct,subprocess,json,hashlib
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_definitions.py')))
root=ctx['root'];archive=ctx['archive'];j=ctx['j'];u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];probe=ctx['probe']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
entry=int(re.search(r'_rf_emitter_definition_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='emitters.tbl');table=archive[e['offset']:e['offset']+e['size']]
names=re.findall(rb'(?im)^\s*\$name:\s*"([^"]*)"',table)
assert len(names)==52 and len(table)<65536
expected=[f[2] for f in ctx['fixtures'][25:77]];fixtures=[]
for name,want in zip(names,expected):
 for query in (name,name.upper(),name.lower()):fixtures.append((table,query,0,want))
for name in (b'missing_emitter',names[0]+b'?',names[0]+b' '):fixtures.append((table,name,-3,bytes([0xa5])*184))
fixtures.append((table,b'',-4,bytes([0xa5])*184))
# First matching name wins; malformed selected blocks preserve output.
block=ctx['fixtures'][25][0]
fixtures.append((b'#Emitters\n$Name: "fixture"\n'+block+b'\n$Name: "fixture"\n$pos: broken\n#End',b'fixture',0,expected[0]))
fixtures.append((b'#Emitters\n$Name: "fixture"\n$pos: broken\n#End',b'fixture',-2,bytes([0xa5])*184))
commands=bytearray();all_expected=bytearray()
for raw,name,status,want in fixtures:
 commands.extend(struct.pack('<I',len(raw))+name.ljust(128,b'\0')+raw);all_expected.extend(struct.pack('<i',status)+want)
 u.mem_write(base,bytes([0xa5])*184);u.mem_write(base+256,name+b'\0');u.mem_write(base+0x1000,raw)
 u.mem_write(stack,struct.pack('<5I',stop,base+0x1000,len(raw),base+256,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=10000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==status&0xffffffff,(name,status)
 # Invalidate source storage before inspecting the owned record.
 u.mem_write(base+0x1000,bytes([0xdd])*len(raw));assert bytes(u.mem_read(base,184))==want,name
assert subprocess.check_output([str(probe),'--emitter-definition'],input=commands)==all_expected
for name,want in zip(names,expected):
 for budget in (len(table)-1,len(table)):
  got=subprocess.check_output([str(probe),'--emitter-load',str(root/'Installed_Game/tables.vpp'),name.decode(),str(budget)])
  assert got==(struct.pack('<i',-4)+bytes([0xa5])*184 if budget<len(table) else bytes(4)+want),(name,budget)
report=dict(result='PASS',names=len(names),lookup_cases=len(fixtures),archive_cases=104,table_bytes=len(table),table_sha256=hashlib.sha256(table).hexdigest(),scope='PC and compiled NXDK named lookup matches independently decoded authored records; source invalidation preserves copies. PC archive binding checks exact/one-byte-short scratch budgets and closes archive before output. No original name-lookup or native XEMU allocation equivalence claimed.')
(root/'artifacts/emitter-loading-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
