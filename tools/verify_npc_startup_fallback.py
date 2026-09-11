"""Disabled movement must affect actual startup animation, not just a slot field."""
import json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];game=root/'Installed_Game'
command=[str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--catalog',str(game/'levels1.vpp'),str(game/'tables.vpp'),str(game/'motions.vpp'),str(game/'meshes.vpp'),'L1S1.rfl']
results={}
for mode in ('baseline','disabled','direct'):
 env=dict(os.environ);env.pop('RF_TEST_START_FALLBACK',None)
 if mode!='baseline':env['RF_TEST_START_FALLBACK']=mode
 output=subprocess.check_output(command,env=env,text=True)
 results[mode]=[line for line in output.splitlines() if line.startswith('STARTUP_POSE\t')]
 assert len(results[mode])==78
assert results['disabled']==results['direct'],'Disabled descriptors did not use slot0 mode3'
changed=sum(a!=b for a,b in zip(results['baseline'],results['disabled']))
assert changed>0,'Fixture did not distinguish animation selections'
report=dict(result='PASS',actors=78,changed=changed,scope='PC owned startup animation regression: all nonzero descriptors disabled via low-byte-zero enabled256, slot0 mode3. Full playback/bone/cache rows equal directly authored slot0 and differ from normal descriptors. Uses original-verified descriptor selector; synthetic fallback case is not an original full-factory comparison. Release checks remain active.')
(root/'artifacts/npc-startup-fallback.json').write_text(json.dumps(report,indent=2));print(report)
