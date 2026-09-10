"""Check integrated level particle ownership, bindings and exact resident budgets."""
import json
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
inventory = json.loads((root/'artifacts/level-emitters.json').read_text())['results']
rooms = {(r['level'], r['uid']): r['room'] for r in json.loads(
    (root/'artifacts/level-emitter-binding.json').read_text())['results']}
materials = {r['level']: r for r in json.loads(
    (root/'artifacts/level-particle-materials-verification.json').read_text())['results']}

def run(level, budget):
    return subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),
        '--level-particles', str(root/'Installed_Game'/level['archive']), level['file'],
        str(root/'Installed_Game'), str(budget)])

results = []
for level in inventory:
    raw = run(level, 1024*1024)
    assert struct.unpack_from('<i', raw)[0] == 0, level['file']
    count, unique, resident, live, seed, state_bytes = struct.unpack_from('<6I', raw, 4)
    assert count == len(level['records']) and len(raw) == 28+count*236
    assert unique == materials[level['file']]['unique']
    assert resident == state_bytes+8+materials[level['file']]['resident_bytes']
    assert live == sum((r['emitter_flags'] & 18) == 18 for r in level['records'])
    names = []
    for i, record in enumerate(level['records']):
        name = record['bitmap'].lower()
        if name not in names:
            names.append(name)
        uid, texture = struct.unpack_from('<2I', raw, 28+i*236)
        assert (uid, texture) == (record['uid'], names.index(name))
        slot = raw[36+i*236:36+i*236+228]
        u32 = lambda offset: struct.unpack_from('<I', slot, offset)[0]
        assert u32(0) == 0 and u32(72) == rooms[level['file'], uid]+1
        assert u32(120) == texture and u32(124) == 1
        assert u32(172) & 255 == bool(record['enabled'])
        assert u32(208) == uid and u32(224) == 1
        assert u32(216) == (i+1 if i+1<count else 129)
        assert u32(220) == (i-1 if i else 129)
    assert run(level, resident) == raw
    assert run(level, resident-1) == struct.pack('<i', -4)
    results.append(dict(level=level['file'], emitters=count, resident_bytes=resident,
        initial_particles=live, final_rng=seed, state_bytes=state_bytes))
report = dict(result='PASS', levels=len(results), emitters=sum(r['emitters'] for r in results),
    maximum_level_bytes=max(r['resident_bytes'] for r in results),
    l1s1_bytes=next(r['resident_bytes'] for r in results if r['level']=='L1S1.rfl'),
    scope='PC shared level runtime loading: authored UID/room/texture/enable bindings, active links, '
          'initial particle counts, deterministic repeat, exact budget and one-byte-under failure. '
          'Probe checks heap pointers, pixel access after source closure and repeated cleanup. '
          'No original full-loader equivalence, native residency or campaign ticking/rendering claim.')
(root/'artifacts/level-particles-verification.json').write_text(
    json.dumps(dict(report=report, results=results), indent=2)+'\n')
print(report)
