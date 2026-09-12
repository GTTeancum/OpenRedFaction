"""Live campaign packed lightmaps versus independent authored RGB conversion."""
import hashlib,json,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];inventory=json.loads((root/'artifacts/inventory.json').read_text());levels=json.loads((root/'artifacts/levels.json').read_text())
probe=root/'build/pc/Release/rf_lightmap_probe.exe';rows=[]
for name in ['L1S1.rfl','L1S2.rfl','L1S3.rfl']:
 level=next(l for l in levels if l['file']==name);archive=next(a for a in inventory['files'] if a['path']==level['archive']);entry=next(e for e in archive['vpp']['entries'] if e['name']==name);section=next(s for s in level['sections'] if s['type']=='0x1200')
 path=root/'Installed_Game'/level['archive']
 with path.open('rb') as f:f.seek(entry['offset']+section['offset']+8);raw=f.read(section['size'])
 count=struct.unpack_from('<I',raw)[0];at=4;expected=[];pixel_bytes=0
 for image in range(count):
  width,height=struct.unpack_from('<2I',raw,at);at+=8;hash_value=2166136261
  for pixel in range(width*height):
   r,g,b=(max(v>>3,4) for v in raw[at:at+3]);at+=3;value=0x8000|(r<<10)|(g<<5)|b
   for v in [value&255,value>>8]:hash_value=((hash_value^v)*16777619)&0xffffffff
  expected.append([width,height,hash_value]);pixel_bytes+=width*height*2
 assert at==len(raw);required=count*20+pixel_bytes
 def run(budget):return subprocess.run([str(probe),str(path),name,str(budget)],capture_output=True,text=True)
 result=run(required);assert result.returncode==0,(name,result.stdout,result.stderr)
 output=[list(map(int,line.split())) for line in result.stdout.splitlines()]
 assert output==[[count,required]]+expected,(name,output[:2],expected[:1])
 short=run(required-1);assert short.returncode==1 and short.stdout.strip()=='-4',(name,short.stdout)
 rows.append(dict(level=name,images=count,packed_pixel_bytes=pixel_bytes,accounted_bytes=required,previous_rgba_bytes=count*20+pixel_bytes*2,saved_bytes=pixel_bytes))
report=dict(result='PASS',pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),rows=rows,scope='Actual rf_lightmaps_open used by campaign, three opening levels. Every packed byte contributes to independent original1555 FNV hash per image; exact and one-byte-short allocation budgets verified. Counts include image descriptors and pixels, exclude owner header, allocator and2560-byte scratch. Native render/PC behavior separately verified.')
(root/'artifacts/campaign-lightmaps-packed.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
