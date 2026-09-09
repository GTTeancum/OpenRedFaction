"""Compare bounded C asset metadata selection with installed table declarations."""
import json,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='tables.vpp');entry=next(e for e in archive['vpp']['entries'] if e['name'].lower()=='entity.tbl')
with (root/'Installed_Game/tables.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size']).decode('cp1252')
text=re.sub(r'"[^"\r\n]*"|//[^\r\n]*',lambda m:'' if m[0].startswith('//') else m[0],raw)
names=list(re.finditer(r'(?im)^\s*\$Name:\s*"([^"]+)"',text));cases=0;report=[]
for i,match in enumerate(names):
    block=text[match.end():names[i+1].start() if i+1<len(names) else len(text)]
    model_match=re.search(r'(?i)\$V3D\s+Filename:\s*"([^"]*)"',block)
    model=model_match[1] if model_match else ''
    skins=[('',[])]+[(m[1],re.findall(r'"([^"]*)"',m[2])) for m in re.finditer(r'(?i)\$Skin:\s*"([^"]+)"\s*\(([^)]*)\)',block)]
    for skin,textures in skins:
        actual=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),str(root/'Installed_Game/tables.vpp'),match[1].upper(),skin.upper()],text=True).splitlines()
        assert actual==[model]+textures,(match[1],skin,actual,[model]+textures)
        cases+=1
    report.append(dict(name=match[1],model=model,skins={k:v for k,v in skins if k}))
result=dict(result='PASS',classes=len(names),selections=cases,assets=report,scope='Installed entity.tbl declarations including commented-out skins; ASCII-insensitive selection and authored replacement order; no compiled-filename conversion or original table-parser equivalence claim')
(root/'artifacts/entity-assets-verification.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k!='assets'})
