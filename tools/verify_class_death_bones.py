"""Loaded opening-class binding against original-audited model bone indices."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
subprocess.run(['python',str(root/'tools/verify_death_bones.py')],check=True)
original={k.lower():v for k,v in json.loads((root/'artifacts/death-bones-verification.json').read_text())['installed'].items()}
rows=[]
for level in ('L1S1.rfl','L1S2.rfl','L1S3.rfl'):
    output=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--skeletons',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'meshes.vpp'),level],text=True)
    for line in output.splitlines():
        if not line.startswith('CLASS_DEATH_BONES\t'):continue
        _,kind,flags,model,*indices=line.split('\t');kind=int(kind);flags=int(flags);indices=list(map(int,indices))
        enabled=kind==2 and bool(flags&0x20000)
        expected=original[model.lower()] if enabled else [-1]*3
        assert indices==expected,(level,model,indices,expected)
        rows.append(dict(level=level,model=model,humanoid=enabled,indices=indices))
report=dict(result='PASS',cases=len(rows),humanoid=sum(r['humanoid'] for r in rows),rows=rows,scope='PC loaded class/skeleton binding; three opening levels, original-audited model indices and reconstructed humanoid/kind gate, forced non-humanoid/non-skeletal branches. Not full original setup or live death.')
(root/'artifacts/class-death-bones-verification.json').write_text(json.dumps(report,indent=2));print(report)
