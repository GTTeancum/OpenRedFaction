"""Explosion recipe metadata against authored field oracle, PC/NXDK."""
import runpy,json,re,struct,subprocess
from pathlib import Path
ctx=runpy.run_path(str(Path(__file__).with_name('verify_particle_definitions.py')));root=ctx['root'];u=ctx['u'];base=ctx['base'];stack=ctx['stack'];stop=ctx['stop'];probe=ctx['probe']
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
e=next(e for a in ctx['j']['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='explosion.tbl');table=ctx['archive'][e['offset']:e['offset']+e['size']]
clean=re.sub(rb'//[^\n]*',b'',table);blocks=re.findall(rb'(?ims)^\s*\$name:\s*"([^"]*)"(.*?)(?=^\s*\$name:|^\s*#end|\Z)',clean)
entry=int(re.search(r'_rf_explosion_recipe_read\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16);fixtures=[]
for name,body in blocks:
 def scalar(data,label,default=0):
  m=re.search(rb'(?im)^\s*'+re.escape(label)+rb'\s*([^\r\n]+)',data);return float(m[1]) if m else default
 central=re.findall(rb'(?ims)\+Central_Emitter:\s*"([^"]*)"(.*?)(?=\+Central_Emitter:|\$|\Z)',body)
 expected=name.ljust(32,b'\0')+struct.pack('<IIf',int(b'no_trails' in body),len(central),scalar(body,b'$Explosion_Play_Time:'))
 rand=0
 for emitter,fields in central:
  process=re.search(rb'(?im)\+process_per_frame:\s*(\w+)',fields)[1].lower() in (b'yes',b'true',b'1')
  expected+=emitter.ljust(64,b'\0')+struct.pack('<Iff',process,scalar(fields,b'+min_size_before_use:'),scalar(fields,b'+play_time_factor:',struct.unpack('<f',struct.pack('<I',0x7f7fffff))[0]))
  rand=scalar(fields,b'+rand_pos_factor:')
 expected+=bytes(76*(6-len(central)));present=0;extras=[];head_time=head_rand=0;sparks_count=0
 for bit,label in enumerate((b'Sparks',b'Trail_Head',b'Trail_Tail')):
  m=re.search(rb'(?ims)\$'+label+rb'_Emitter:\s*"([^"]*)"(.*?)(?=\$|\Z)',body)
  extras.append(m[1].ljust(64,b'\0') if m else bytes(64))
  if m:
   present|=1<<bit
   if bit==0:sparks_count=int(scalar(m[2],b'+number:'))
   if bit==1:head_time=scalar(m[2],b'+time_to_emit_head_parts:');head_rand=scalar(m[2],b'+rand_pos_factor:')
 expected+=b''.join(extras)+struct.pack('<iIfff',sparks_count,present,rand,head_time,head_rand);assert len(expected)==712
 for query in (name,name.upper()):
  fixtures.append((table,query,0,expected))
  got=subprocess.check_output([str(probe),'--explosion-load',str(root/'Installed_Game/tables.vpp'),query.decode(),str(len(table))]);assert got==bytes(4)+expected,(query,struct.unpack('<i',got[:4]),[(i,a,b) for i,(a,b) in enumerate(zip(got[4:],expected)) if a!=b][:20])
 short=subprocess.check_output([str(probe),'--explosion-load',str(root/'Installed_Game/tables.vpp'),name.decode(),str(len(table)-1)]);assert short==struct.pack('<i',-4)+bytes([0xa5])*712
prefix=b'#explosion types\n$Name: "test"\n$Flags: ()\n$Explosion_Play_Time: 2\n'
central=b'+Central_Emitter: "fixture"\n+process_per_frame: yes\n'
for data,status in ((prefix+central*7,-4),(prefix+b'$Unknown: 1',-2),(prefix+b'$Sparks_Emitter: "s"',-2),(prefix+b'$Trail_Head_Emitter: "h"',-2),(prefix+b'+Central_Emitter: "x"',-2),(prefix+b'+Central_Emitter: "x"\n+process_per_frame: maybe',-2),(prefix+central+b'+play_time_factor: nan',-2)):
 fixtures.append((data,b'test',status,bytes([0xa5])*712))
fixtures.append((table,b'no such recipe',-3,bytes([0xa5])*712))
for raw,name,status,want in fixtures:
 u.mem_write(base,bytes([0xa5])*712);u.mem_write(base+768,name+b'\0');u.mem_write(base+0x1000,raw)
 u.mem_write(stack,struct.pack('<5I',stop,base+0x1000,len(raw),base+768,base));u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=2000000)
 assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==status&0xffffffff,(name,status,u.reg_read(UC_X86_REG_EAX))
 u.mem_write(base+0x1000,bytes([0xdd])*len(raw));assert bytes(u.mem_read(base,712))==want,name
assert len(blocks)==9
report=dict(result='PASS',recipes=9,pc_named_cases=18,pc_short_budget_cases=9,nxdk_cases=len(fixtures),scope='Independent authored recipe field oracle, PC archive load and compiled NXDK reader. Malformed NXDK inputs preserve output. Supplied-parser original defaults were separately verified. Emitter resolution, resource ownership and live explosion execution are excluded.')
(root/'artifacts/explosion-recipes-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
