"""Resolved explosion metadata ownership and archive budget checks."""
import runpy,struct,re,json,subprocess
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_explosion_recipes.py')));root=ctx['root'];u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];probe=ctx['probe'];particle=ctx['ctx']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
e=next(e for a in particle['j']['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='emitters.tbl');table=particle['archive'][e['offset']:e['offset']+e['size']]
names=re.findall(rb'(?im)^\s*\$name:\s*"([^"]*)"',table)
values=[f[2] for f in particle['fixtures'][25:77]]
prepared=subprocess.check_output([str(probe),'--particle-prepare'],input=b''.join(values));lookup={n.lower():prepared[i*184:(i+1)*184] for i,n in enumerate(names)}
entry=int(re.search(r'_rf_explosion_definition_resolve\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
checks=[];bound_cases=0
for _,query,_,recipe in ctx['fixtures'][:18:2]:
 count=struct.unpack_from('<I',recipe,36)[0];present=struct.unpack_from('<I',recipe,696)[0]
 slot_names=[recipe[44+i*76:108+i*76].split(b'\0')[0] if i<count else b'' for i in range(6)]
 slot_names += [recipe[500+i*64:564+i*64].split(b'\0')[0] if present&(1<<i) else b'' for i in range(3)]
 mask=sum(1<<i for i,n in enumerate(slot_names) if n and n.lower() in lookup)
 expected=recipe+b''.join(lookup[n.lower()] if n and n.lower() in lookup else bytes(184) for n in slot_names)+struct.pack('<3I',mask,2380,2380)
 checks.append((recipe,0,expected))
 want_load=expected[:-4]+struct.pack('<I',2380+len(table))
 for budget in (2380+len(table),2380+len(table)-1):
  got=subprocess.check_output([str(probe),'--explosion-definition',str(root/'Installed_Game/tables.vpp'),query.decode(),str(budget)])
  assert got==(bytes(4)+want_load if budget==2380+len(table) else struct.pack('<i',-4)+bytes([0xa5])*2380),query
  bound_cases+=1
# Missing central is an error; optional missing leaves its resolution bit clear.
recipe=bytearray(checks[0][0]);recipe[44:108]=b'absent central'.ljust(64,b'\0');checks.append((bytes(recipe),-3,bytes([0xa5])*2380))
recipe=bytearray(checks[0][0]);recipe[500:564]=b'absent sparks'.ljust(64,b'\0');want=bytearray(checks[0][2]);want[:712]=recipe;want[712+6*184:712+7*184]=bytes(184);struct.pack_into('<I',want,2368,struct.unpack_from('<I',want,2368)[0]&~64);checks.append((bytes(recipe),0,bytes(want)))
recipe=bytearray(checks[0][0]);struct.pack_into('<I',recipe,36,7);checks.append((bytes(recipe),-4,bytes([0xa5])*2380))
for recipe,status,want in checks:
 u.mem_write(base,bytes([0xa5])*2380);u.mem_write(base+0xc00,recipe);u.mem_write(base+0x1000,table)
 u.mem_write(stack,struct.pack('<5I',stop,base+0xc00,base+0x1000,len(table),base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=50000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==status&0xffffffff,(hex(u.reg_read(UC_X86_REG_EIP)),u.reg_read(UC_X86_REG_EAX),status)
 u.mem_write(base+0xc00,bytes([0xdd])*712);u.mem_write(base+0x1000,bytes([0xdd])*len(table));assert bytes(u.mem_read(base,2380))==want
report=dict(result='PASS',recipes=9,nxdk_cases=len(checks),pc_budget_cases=bound_cases,resident_bytes=2380,peak_bytes=2380+len(table),scope='Composition of separately validated recipes, emitter metadata and direction preparation. PC archive closed before inspection; NXDK source storage overwritten. Missing central vs optional resolution and over-capacity recipe checked. No bitmap residency or active explosion execution.')
(root/'artifacts/explosion-definitions-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
