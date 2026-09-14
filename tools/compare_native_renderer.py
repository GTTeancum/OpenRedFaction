"""Compare paired native renderer variants, including every final RGB pixel."""
import argparse
import json
import hashlib
import struct
from pathlib import Path
from PIL import Image, ImageChops

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('reference', type=Path)
p.add_argument('optimized', type=Path)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
off, on = [json.loads((run / 'report.json').read_text()) for run in (a.reference, a.optimized)]
assert off['result'] == on['result'] == 'PASS'
assert any(off.get(key) != on.get(key) for key in ('model_culling', 'command_batching', 'world_grouping')), 'No renderer variant changed'
for key in ('frames', 'actor', 'pc_sha256'):
    assert off[key] == on[key], key
# cxbe stamps the XBE header and certificate on every repack. Compare every
# other byte, including all loadable code/data, instead of ignoring section data.
# Offsets: installed nxdk/tools/cxbe/Xbe.h Header and Certificate definitions.
def executable_bytes(run):
    data = bytearray((run / 'default.xbe').read_bytes())
    assert data[:4] == b'XBEH'
    base = struct.unpack_from('<I', data, 0x104)[0]
    certificate = struct.unpack_from('<I', data, 0x118)[0] - base
    assert 0x178 <= certificate < len(data) - 8
    for offset in (0x114, certificate + 4):
        data[offset:offset+4] = bytes(4)
    return data
normalized = [executable_bytes(run) for run in (a.reference, a.optimized)]
assert normalized[0] == normalized[1], 'XBE differs beyond packaging timestamps'
assert off['checks'].keys() == on['checks'].keys()
for label in off['checks']:
    assert off['checks'][label] == on['checks'][label], label
images = [Image.open(run / 'framebuffer.png').convert('RGB') for run in (a.reference, a.optimized)]
diff = ImageChops.difference(*images)
snapshots = [json.loads((run / 'guest-memory-final.json').read_text())['symbols'] for run in (a.reference, a.optimized)]
before, after = [s['rf_xbox_retained_models']['words'] for s in snapshots]
visibility = snapshots[1]['rf_xbox_model_visibility']['words']
world = [s['rf_xbox_retained_world']['words'] for s in snapshots]
assert world[0][:6] == world[1][:6], 'World cache or visible geometry changed'
assert world[0][7] == world[1][7] == 0, 'Unexpected world fallback'
prior_visibility = snapshots[0]['rf_xbox_model_visibility']['words']
assert before[0] + sum(prior_visibility[2:4]) == after[0] + sum(visibility[2:4]), 'Unexpected queue difference'
assert before[6] == after[6] == 0, 'Unexpected CPU fallback'
report = dict(result='PASS' if diff.getbbox() is None else 'FAIL', frames=on['frames'], actor=on['actor'],
    differing_pixels=sum(pixel != (0, 0, 0) for pixel in zip(*[iter(diff.tobytes())]*3)), difference_box=diff.getbbox(),
    before_retained=before, after_retained=after, visibility=visibility,
    retained_world=world,
    command_batching=[run['command_batching'] for run in (off, on)],
    world_grouping=[run.get('world_grouping') for run in (off, on)],
    model_culling=[run['model_culling'] for run in (off, on)],
    world_groups=[s.get('rf_xbox_world_groups', {}).get('words') for s in snapshots],
    command_blocks=[s['rf_xbox_command_blocks']['words'] for s in snapshots],
    bounds_poses=snapshots[1]['rf_xbox_bounds_poses']['words'],
    normalized_xbe_sha256=hashlib.sha256(normalized[0]).hexdigest(),
    scope='Same compiled XBE and input/camera fixture. Selected PC gameplay checks and full native RGB framebuffer. '
          'One final frame; not full animation/campaign coverage or a sustained FPS comparison.')
a.out.parent.mkdir(parents=True, exist_ok=True)
a.out.write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
assert report['result'] == 'PASS', 'Native framebuffer changed'
