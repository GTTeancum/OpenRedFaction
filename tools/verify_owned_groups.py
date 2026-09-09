"""Owned group data survives archive closure and respects exact memory budgets."""
import ctypes as c,json,runpy,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];reader=runpy.run_path(str(root/'tools/verify_group_reader.py'));Group=reader['Group'];Key=reader['Key'];results=[]
for level in reader['levels']:
 args=[str(root/'build/pc/Release/rf_collision_probe.exe')]
 source=subprocess.check_output(args+['--groups',str(root/'Installed_Game'/level['archive']),level['file']])
 owned=subprocess.check_output(args+['--owned-groups',str(root/'Installed_Game'/level['archive']),level['file']]);count,allocated=struct.unpack_from('<2I',owned)
 assert count==len(level['records']);at=8;src=0;budget=16+1368*count;legacy_count=0
 for reference in level['records']:
  g=Group.from_buffer_copy(owned,at);n=c.sizeof(Group)+g.key_count*c.sizeof(Key)
  assert owned[at:at+n]==source[src:src+n],(level['file'],'record/keys');at+=n;src+=n;budget+=g.key_count*c.sizeof(Key)
  for legacy in reference['legacy']:
   want=struct.pack('<I12f',legacy['uid'],*legacy['position'],*legacy['orientation_disk'])
   assert owned[at:at+52]==want,(level['file'],'legacy pose');at+=52;budget+=52;legacy_count+=1
  n=4*sum(g.ids_count);assert owned[at:at+n]==source[src:src+n],(level['file'],'UID lists');at+=n;src+=n;budget+=n
 assert at==len(owned) and src==len(source) and allocated==budget,(level['file'],allocated,budget)
 results.append(dict(file=level['file'],groups=count,legacy=legacy_count,allocated_bytes=allocated))
report=dict(result='PASS',levels=len(results),groups=sum(r['groups'] for r in results),legacy=sum(r['legacy'] for r in results),max_allocated=max(r['allocated_bytes'] for r in results),scope='PC full owned controller records/keys/UID lists match independently checked reader; legacy bytes match independent inventory. Archive closed and level overwritten before serialization, exact/one-byte-short budgets and failure preservation checked, including truncated section; repeated close. No runtime registration or XEMU.',results=results)
(root/'artifacts/owned-groups-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
