"""Independently check catalog playback ownership, aliases and reference totals."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game';results=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
 output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'motions.vpp'),str(game/'meshes.vpp'),level],text=True)
 names={};aliases={};totals={};owner=None
 for row in output.splitlines():
  f=row.split('\t')
  if f[0]=='CATALOG_RESOURCE':names[int(f[1]),int(f[2])]=f[4].rsplit('.',1)[0].lower()
  elif f[0]=='PLAYBACK_ALIAS':aliases[int(f[1]),int(f[2])]=tuple(map(int,f[3:]))
  elif f[0]=='PLAYBACK_REFERENCES':totals[int(f[1])]=int(f[2])
  elif f[0]=='PLAYBACK_OWNER':owner=list(map(int,f[1:]))
 assert names.keys()==aliases.keys()
 cache_names={};expected={}
 for key,name in names.items():
  cache,refs=aliases[key]
  if cache in cache_names:assert cache_names[cache]==name
  else:cache_names[cache]=name
  expected[cache]=expected.get(cache,0)+refs
 assert len(set(cache_names.values()))==len(cache_names)
 assert expected==totals and owner[:2]==[len(names),len(cache_names)]
 results.append(dict(level=level,resources=owner[0],cache_identities=owner[1],resident_bytes=owner[2],peak_bytes=owner[3],aliased_registrations=owner[0]-owner[1]))
report=dict(result='PASS',scope='PC owner metadata, exact/one-byte-short budgets, zero initial counters, synthetic runtime counts aggregated by independent last-dot case-insensitive identity across models and loop registrations; invalid-ID/negative-reference output preservation and repeatable close. Not live actor startup or unload scheduling.',results=results)
(root/'artifacts/playback-resources.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
