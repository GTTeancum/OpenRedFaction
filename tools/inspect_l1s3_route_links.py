"""Read authored route links and test the observed pit endpoint against its volume.
This diagnoses missing campaign systems; it does not certify intended traversal.
"""
import hashlib,json,struct,subprocess
from pathlib import Path
from inspect_levels import inspect as inspect_level
from inspect_triggers import inspect as inspect_triggers
from inspect_events import inspect as inspect_events
root=Path(__file__).resolve().parents[1]
inventory=json.loads((root/'artifacts/inventory.json').read_text())
archive=next(a for a in inventory['files'] if a['path']=='levels1.vpp')
entry=next(e for e in archive['vpp']['entries'] if e['name']=='L1S3.rfl')
with (root/'Installed_Game/levels1.vpp').open('rb') as f:
    level=inspect_level(f,entry);f.seek(entry['offset']);raw=f.read(entry['size'])
def section(kind):
    s=next(s for s in level['sections'] if s['type']==kind)
    return raw[s['offset']+8:s['offset']+8+s['size']]
# Event inventory uses original 5a1a3c names; parser verifies each authored type.
types=json.loads((root/'artifacts/events.json').read_text())['types']
triggers=inspect_triggers(section('0x60000'));events=inspect_events(section('0x600'),types)
events_by_uid={e['uid']:e for e in events}
selected=[]
for uid in (357,9322,9323,9350):
    t=next(t for t in triggers if t['uid']==uid)
    selected.append(dict(trigger=t,event_targets=[events_by_uid[x] for x in t['links'] if x in events_by_uid],other_targets=[x for x in t['links'] if x not in events_by_uid]))
pit=selected[0]['trigger'];assert pit['shape']==1
wire=struct.pack('<I16f',pit['shape'],*pit['position'],0,*pit['orientation_disk'],*pit['dimensions_disk'])
volume=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-volume'],input=wire)
assert len(volume)==72 and struct.unpack_from('<i',volume)[0]==0
v=struct.unpack('<II16f',volume);center=v[2:5];matrix=v[6:15];size=v[15:18]
route=json.loads((root/'artifacts/l1s3-route-observation/report.json').read_text())
assert route['pc_sha256']==hashlib.sha256((root/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(), 'Regenerate route observation for current PC build'
observations=[]
for row in route['checkpoints']:
    pos=row['position'];wire=struct.pack('<15fI9f',*center,*matrix,*size,0,*pos,*pos,*pos)
    result=subprocess.check_output([str(root/'build/pc/Release/rf_event_probe.exe'),'--trigger-box'],input=wire)
    status,inside=struct.unpack('<iI',result);assert status==0
    observations.append(dict(frame=row['frames'],position=pos,inside_pit=inside))
assert next(r for r in observations if r['frame']==930)['inside_pit']==1
report=dict(status='OBSERVED',level_sha256=hashlib.sha256(raw).hexdigest(),pc_sha256=hashlib.sha256((root/'build/pc/Release/rf_event_probe.exe').read_bytes()).hexdigest(),links=selected,checkpoint_contact=observations,scope='Fresh installed L1S3 section parsing, authored trigger volume conversion and ordinary box point contact. At930 the observed player position lies in trigger357 linked to Continuous_Damage9488 (words100000,7). Trigger value_byte2 is skipped by current scene contact integration. Actual damage dispatch, actor filtering, timing, health/death and original gameplay traversal are not verified. Door4 has no proven activation route; nearby trigger9322 requests L1S2, not door activation.')
(root/'artifacts/l1s3-route-links.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(status=report['status'],checkpoint_contact=observations,pit_links=pit['links'],pit_damage=events_by_uid[9488]['words'],east_target=events_by_uid[9324]['texts']),indent=2))
