"""Validate startup inventory/animation composition with actual opening catalogs."""
import json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
subprocess.check_call([sys.executable,str(root/'tools/verify_weapon_supply_binding.py')])
command=[str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog',*[str(root/'Installed_Game'/name) for name in ('levels1.vpp','tables.vpp','motions.vpp','meshes.vpp')],'L1S1.rfl']
result=subprocess.run(command,capture_output=True,text=True);(root/'artifacts/startup-binding-catalog.log').write_text(result.stdout+result.stderr);assert result.returncode==0,result.stderr
weapons=json.loads((root/'artifacts/weapon-supply-binding.json').read_text())['weapons'];rows=[line.split('\t') for line in result.stdout.splitlines() if line.startswith('STARTUP_WEAPON_BINDING\t')];classes={};seen=set()
for _,cls,weapon,loaded,reserve in rows:
 i=int(weapon);record=weapons[i];assert int(loaded)==max(0,record['sp_magazine']);assert int(reserve)==(-1 if record['ammo_index']<0 else record['sp_capacity']);assert (cls,i) not in seen;seen.add((cls,i));classes[cls]=classes.get(cls,0)+1
assert rows and all(count==len(weapons) for count in classes.values())
report=dict(result='PASS',level='L1S1.rfl',classes=len(classes),weapons=len(weapons),composed_cases=len(rows),scope='Loaded PC opening catalogs: startup acquisition/refill and concrete weapon animation selection for every weapon on every skeletal class. Probe compares full selected mapping and all45 sound pointers. Ammo results independently checked against original supply audit. Original startup/overlay primitives have separate PC/NXDK verifiers; composed native scene execution excluded.')
(root/'artifacts/startup-weapon-bindings.json').write_text(json.dumps(report,indent=2));print(report)
