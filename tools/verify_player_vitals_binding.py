"""Check campaign player initialization against independently probed class data."""
import json,struct,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
run=Path(sys.argv[1]);run=run if run.is_absolute() else root/run
text=(run/'pc-reference.txt').read_text()
row=next(list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith('PLAYER_VITALS '))
blob=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),
    '--vitals-config',str(root/'Installed_Game/tables.vpp'),'miner1'])
assert len(blob)==16 and struct.unpack_from('<i',blob)[0]==0
health,armor=struct.unpack_from('<ff',blob,4)
assert health>=0
expected=list(struct.unpack('<4I',struct.pack('<4f',health,armor,health,armor)))
factors=json.loads((root/'artifacts/entity-damage-factors.json').read_text())
assert factors['result']=='PASS'
values=next(item['factors'] for item in factors['classes'] if item['name'].lower()=='miner1')
hash_value=2166136261
for byte in struct.pack('<11f',*values):hash_value=((hash_value^byte)*16777619)&0xffffffff
assert row==expected+[108,hash_value],row
report=dict(result='PASS',class_name='miner1',health=health,armor=armor,factors=values,words=row,
    scope='Opening campaign miner1 class: loaded vitals against separate metadata probe; multiplier hash against original-verified factor report. This is class initialization, not full player creation, respawn, saved-state restoration or damage dispatch.')
(root/'artifacts/player-vitals-binding.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
