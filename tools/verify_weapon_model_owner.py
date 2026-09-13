"""Exercise installed shared weapon geometry/tag ownership and rollback budgets."""
import json,re,struct,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
w=lambda *v:struct.pack('<'+'I'*len(v),*[i&0xffffffff for i in v])
v=json.loads((root/'artifacts/inventory.json').read_text());e=next(e for a in v['files'] if a['path']=='tables.vpp' for e in a['vpp']['entries'] if e['name']=='weapons.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(e['offset']);text=f.read(e['size'])
text=re.sub(rb'"[^"\r\n]*"|//[^\r\n]*',lambda m:b'' if m[0].startswith(b'//') else m[0],text)
files=[]
for block in re.split(rb'\$Name:\s*"[^"\r\n]*"',text)[1:]:
 m=re.search(rb'\$3rd\s+Person\s+V3D:\s*"([^"\r\n]*)"',block);files.append(m[1].decode() if m else '')
probe=str(root/'build/pc/Release/rf_entity_assets_probe.exe')
def run(masks,budget):
 return subprocess.check_output([probe,'--weapon-models',str(root/'Installed_Game/tables.vpp'),str(root/'Installed_Game/meshes.vpp'),str(budget)],input=b''.join(w(mask,mask>>32) for mask in masks))
masks=[0,(1<<len(files))-1]+[1<<i for i in range(len(files))]+[0x5555555555555555,0xaaaaaaaaaaaaaaaa]
raw=run(masks,4*1024*1024);at=0;stats=[]
for mask in masks:
 code,=struct.unpack_from('<i',raw,at);assert code==0;at+=4
 count,resident,peak,tags=struct.unpack_from('<4I',raw,at);at+=16
 slots=struct.unpack_from('<256I',raw,at);at+=1024
 names=[raw[at+i*64:at+(i+1)*64].split(b'\0')[0].decode() for i in range(count)];at+=count*64
 expected=[]
 for i in range(64):
  filename=files[i] if i<len(files) else '';token=0
  if filename and mask&(1<<i):
   compiled=str(Path(filename).with_suffix('.v3m')).lower()
   if compiled not in expected:expected.append(compiled)
   token=expected.index(compiled)+1
  assert slots[i*4:i*4+4]==(int(bool(filename)),token,0xffffffff,0xffffffff),(mask,i)
 assert [s.lower() for s in names]==expected and count==len(expected)
 assert resident<=peak<=4*1024*1024
 stats.append((count,resident,peak,tags))
assert at==len(raw)
# Every selected subset must succeed at its reported peak, and fail one byte below.
# The probe also checks rollback and repeated close after every success/failure.
for mask,record in zip(masks,stats):
 exact=run([mask],record[2]);assert struct.unpack_from('<i',exact)[0]==0
 assert struct.unpack_from('<4I',exact,4)==record
 assert run([mask],record[2]-1)==w(-4)
report=dict(result='PASS',selection_cases=len(masks),budget_cases=len(masks)*2,full=dict(zip(['models','resident_bytes','peak_bytes','tags'],stats[1])),scope='PC installed-archive geometry/tag ownership, alias sharing, all44 single-ID subsets plus empty/full/alternating masks, original-name order, caches, exact peak boundaries, rollback and repeated close. No textures, native XEMU or compiled NXDK owner execution claimed.')
(root/'artifacts/weapon-model-owner.json').write_text(json.dumps(report,indent=2));print(report)
