"""Audit room-culling output from rotated process-local cameras near campaign exits."""
import argparse, hashlib, json, os, struct, subprocess
from pathlib import Path
from PIL import Image, ImageChops

p = argparse.ArgumentParser()
p.add_argument('baseline', type=Path)
p.add_argument('--out', type=Path, default=Path('artifacts/visibility-views'))
a = p.parse_args()
r = Path(__file__).resolve().parents[1]
d = a.out.resolve()
d.mkdir(parents=True, exist_ok=True)
executables = [a.baseline.resolve(), r / 'build/pc/Release/rf_pc_play.exe']
hashes = [hashlib.sha256(x.read_bytes()).hexdigest() for x in executables]
assert hashes[0] != hashes[1]
results = []
for level, exit_uid in [('L1S1.rfl', '9019'), ('L1S2.rfl', '9512'), ('L1S3.rfl', '9324')]:
    camera_hashes = set()
    for name, pitch, yaw in [('left', 0, -1), ('right', 0, 1), ('up-right', .5, .5)]:
        key = level[:-4] + '-' + name
        source = d / (key + '.bin')
        source.write_bytes(b'RFI5' + struct.pack('<I', 44) + b''.join(
            struct.pack('<5f6I', 0, 0, 0, pitch if i >= 30 else 0,
                        yaw if i >= 30 else 0, 0, 0, 0, 0, 0, 0) for i in range(90)))
        env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
        env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_EXIT_START=exit_uid)
        images, states, worlds = [], [], []
        for tag, exe in zip(('baseline', 'culled'), executables):
            output = d / f'{key}-{tag}.ppm'
            run = subprocess.run([str(exe), '--spawn-replay', str(r / 'Installed_Game'),
                                  str(source), str(output)], env=env, capture_output=True, text=True)
            (d / f'{key}-{tag}.log').write_text(run.stdout + run.stderr)
            run.check_returncode()
            assert 'Completed 90 frames' in run.stdout
            states.append([x for x in run.stdout.splitlines() if x.startswith(
                ('PC_PLAY_BODY ', 'LEVEL_TRANSITION ', 'PLAYER_SPAWN ', 'ACTOR_PLAYER_INPUT '))])
            assert any(x.startswith('PC_PLAY_BODY ') for x in states[-1])
            world = next(x for x in run.stdout.splitlines() if x.startswith('ACTOR_FOLLOW_SUMMARY '))
            worlds.append(list(map(int, world.split()[1:])))
            image = Image.open(output).convert('RGB')
            image.save(output.with_suffix('.png'))
            images.append(image)
        assert states[0] == states[1], (key, 'Player state changed')
        body = next(x for x in states[1] if x.startswith('PC_PLAY_BODY '))
        camera_hashes.add(hashlib.sha256(body.encode()).hexdigest())
        assert images[0].size == images[1].size
        changed = sum(x != y for x, y in zip(images[0].get_flattened_data(), images[1].get_flattened_data()))
        difference = ImageChops.difference(*images)
        if changed:
            difference.save(d / f'{key}-difference.png')
        row = dict(case=key, changed_pixels=changed, bounds=difference.getbbox(),
                   baseline_peak_bytes=worlds[0][2], culled_peak_bytes=worlds[1][2],
                   final_body_sha256=hashlib.sha256(body.encode()).hexdigest(), player_state_identical=True)
        results.append(row)
        print(json.dumps(row), flush=True)
        (d / 'report.json').write_text(json.dumps(dict(
            result='IN_PROGRESS', executable_sha256=hashes, cases=results), indent=2))
    assert len(camera_hashes) == 3, 'Camera inputs did not produce distinct final body states'
report = dict(result='REVIEW_REQUIRED' if any(x['changed_pixels'] for x in results) else 'PASS',
              executable_sha256=hashes, cases=results,
              scope='Nine final views after 90-frame rotations near three authored exits; final player state and recorded inputs match. Differences require inspection, not automatic acceptance. No complete campaign or every-frame coverage.')
(d / 'report.json').write_text(json.dumps(report, indent=2))
