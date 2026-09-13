"""Inventory authored v180 mapping records and check complete PC decoding."""
import json,struct,subprocess
from pathlib import Path
from inspect_geometry import inspect
root=Path(__file__).resolve().parents[1]
w=lambda *v:struct.pack('<'+'I'*len(v),*v)
def main():
 inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text());reports=[];total=0
 for level in levels:
  geo=next((s for s in level['sections'] if s['type']=='0x100'),None);images=next((s for s in level['sections'] if s['type']=='0x1200'),None)
  if not geo or not images:continue
  archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==level['file'])
  with (root/'Installed_Game'/level['archive']).open('rb') as f:
   f.seek(entry['offset']+geo['offset']+8);data=f.read(geo['size']);f.seek(entry['offset']+images['offset']+8);image_count=struct.unpack('<I',f.read(4))[0]
  geometry=inspect(data);count=geometry['mappings'];begin=geometry['mapping_offset'];inputs=[];expected=[];special=inhibit=empty=fallback=0
  for i in range(count):
   raw=data[begin+i*96:begin+(i+1)*96];image=struct.unpack_from('<I',raw)[0];flags=struct.unpack_from('<2I',raw,56)
   resolved=image if image<image_count and image<=0x7fffffff else 0
   output=w(resolved,*raw[4:8])+raw[8:56]+w(int(flags[0]!=0),int(flags[1]!=0))+raw[64:76]+raw[84:92]+raw[76:84]+raw[92:96]
   inputs.append(w(image_count)+raw);expected.append(w(0)+output);special+=flags[0]!=0;inhibit+=flags[1]!=0;empty+=not raw[6] or not raw[7];fallback+=resolved!=image
  assert subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--lightmap-mapping-read'],input=b''.join(inputs))==b''.join(expected),level['file']
  reports.append(dict(file=level['file'],archive=level['archive'],mappings=count,images=image_count,special=special,inhibit=inhibit,empty=empty,image_fallbacks=fallback,decoded_bytes=count*108));total+=count
 result=dict(result='PASS',levels=len(reports),pc_records=total,records=reports,scope='Authored static-world v180 mapping inventory and complete PC decoded-record comparison against documented layout. Primitive original/NXDK evidence separate; no native residency or moving-solid records.')
 (root/'artifacts/lightmap-mapping-inventory.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k!='records'});print(next(r for r in reports if r['file']=='L1S1.rfl'));print('largest',max(reports,key=lambda r:r['decoded_bytes']))
if __name__=='__main__':main()
