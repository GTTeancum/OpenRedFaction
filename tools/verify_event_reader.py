"""Compare shared C event fields/links to independent original-layout inventory."""
import json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/events.json').read_text());results=[]
def string(s):
 b=s.encode('cp1252');assert len(b)<256 and b'\0' not in b
 return b+bytes(256-len(b))
for level in inventory['results']:
 raw=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game'/level['archive']),level['file'],'--events'])
 count,=struct.unpack_from('<I',raw);assert count==len(level['records']);at=4
 for r in level['records']:
  orientation=r['orientation_disk'];link_offset=r['offset']+r['bytes']-4-(36 if orientation else 0)-len(r['links'])*4
  want=struct.pack('<15I',r['uid'],r['offset'],r['bytes'],link_offset,len(r['links']),r['header_byte'],*r['flags'],*r['words'],*r['color_bytes'],int(orientation is not None))
  want+=string(r['type'])+string(r['name'])+b''.join(string(s) for s in r['texts'])
  want+=struct.pack('<15f',r['delay'],*r['position'],*r['values'],*(orientation or [0]*9))
  assert len(want)==1144 and raw[at:at+1144]==want,(level['file'],r['uid']);at+=1144
  links=struct.pack('<'+'I'*len(r['links']),*r['links'])
  assert raw[at:at+len(links)]==links;at+=len(links)
 assert at==len(raw)
 results.append(dict(file=level['file'],events=count,links=sum(len(r['links']) for r in level['records'])))
report=dict(result='PASS',levels=len(results),events=sum(r['events'] for r in results),links=sum(r['links'] for r in results),scope='PC C reader vs independent Python inventory: every preserved field and ordered link, each record one-byte truncation with output/cursor preservation, link bounds and exact EOF. No original parser execution, owned events or runtime actions.',results=results)
(root/'artifacts/event-reader-verification.json').write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='results'})
