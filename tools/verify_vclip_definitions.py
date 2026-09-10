"""Owned vclip definitions vs independent authored field oracle, PC/NXDK."""
import runpy,re,struct,json,subprocess
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_definitions.py')));root=ctx['root'];j=ctx['j'];archive=ctx['archive'];u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];probe=ctx['probe']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
e=next(e for a in j['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='vclip.tbl');table=archive[e['offset']:e['offset']+e['size']]
clean=re.sub(rb'//[^\n]*',b'',table);blocks=re.findall(rb'(?ims)^\s*\$name:\s*"([^"]*)"(.*?)(?=^\s*\$name:|^\s*#end|\Z)',clean)
entry=int(re.search(r'_rf_vclip_definition_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);particle_index=0;count=0;first={}
for name,block in blocks:
 fields={re.sub(rb'[ _\t]',b'',k.lower()):v.strip() for k,v in re.findall(rb'\$([^:\r\n]+):([^\r\n]*)',block)}
 flags=0
 for bit,label in enumerate((b'liquid_surface',b'radius_in_multiples',b'no_z_check',b'code_explode')):
  if label in fields.get(b'flags',b'').lower():flags|=1<<bit
 def string(k,size):return fields.get(k,b'""').strip(b'"').ljust(size,b'\0')
 expected=name.ljust(64,b'\0')+string(b'vbmfilename',64)+string(b'vfxfilename',64)+string(b'explosionname',32)+string(b'foleysound',64)
 expected+=struct.pack('<IIffiII',flags,fields.get(b'vbmglow',b'false').lower() in (b'true',b'yes',b'1'),float(fields.get(b'damage',0)),float(fields.get(b'vfxradius',20)),int(fields.get(b'particlecount',0)),b'particlecount' in fields,b'foleysound' in fields)
 if b'particlecount' in fields:expected+=ctx['fixtures'][particle_index][2];particle_index+=1
 else:expected+=bytes(184)
 assert len(expected)==500
 expected=first.setdefault(name.lower(),expected)
 for query in (name,name.upper()):
  u.mem_write(base,bytes([0xa5])*500);u.mem_write(base+512,query+b'\0');u.mem_write(base+0x1000,table)
  u.mem_write(stack,struct.pack('<5I',stop,base+0x1000,len(table),base+512,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=2000000)
  assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==0,(query,u.reg_read(UC_X86_REG_EAX))
  u.mem_write(base+0x1000,bytes([0xdd])*len(table));assert bytes(u.mem_read(base,500))==expected,query
  got=subprocess.check_output([str(probe),'--vclip-load',str(root/'Installed_Game/tables.vpp'),query.decode(),str(len(table))]);assert got==bytes(4)+expected,query;count+=1
 short=subprocess.check_output([str(probe),'--vclip-load',str(root/'Installed_Game/tables.vpp'),name.decode(),str(len(table)-1)]);assert short==struct.pack('<i',-4)+bytes([0xa5])*500
 assert name!=b'charge_explode' or (string(b'explosionname',32).rstrip(b'\0')==b'rocket hit' and flags==8)
assert len(blocks)==63 and particle_index==25
report=dict(result='PASS',definitions=63,lookup_cases=count,short_budget_cases=63,particle_blocks=25,scope='PC archive loads and compiled-NXDK named reads compared to independent authored field oracle; 25 embedded particle records match prior oracle. Source storage overwritten or closed before inspection. Original parser, Foley handles and explosion consumers not implemented.')
(root/'artifacts/vclip-definitions-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
