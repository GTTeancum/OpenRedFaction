"""Process-local authored post checkpoint continuation; no host input or emulator."""
import argparse
import struct
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def check_post_image(folder):
    from PIL import Image
    # Fixed measured middle-shot camera. Exclude weapon/HUD, whose animation
    # clocks restart; include the entire projected post and surrounding gap.
    box = (300, 180, 338, 320)
    images = [Image.open(folder / (name + '.ppm')).convert('RGB') for name in ('continued', 'control')]
    assert all(image.size == (640, 480) for image in images), 'Unexpected post capture size'
    pixels = [image.crop(box).tobytes() for image in images]
    assert pixels[0] == pixels[1], 'Post rendering differs after reload despite matching state'
    return {'box': box, 'pixels': 38 * 140, 'sha256': hashlib.sha256(pixels[0]).hexdigest(),
            'scope': 'Exact fixed-camera post region; no whole-frame, motion or audio acceptance'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=('two-shot', 'reset-zero', 'middle-shot', 'retired-piece', 'support-loss'), default='two-shot')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/pc')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    reset = args.case == 'reset-zero'
    middle = args.case == 'middle-shot'
    retired = args.case == 'retired-piece'
    support = args.case == 'support-loss'
    folder = ROOT / ('artifacts/authored-post-live/solo-reset-continuation' if reset else
                     'artifacts/authored-post-live/solo-continuation')
    if middle:
        folder = ROOT / 'artifacts/authored-post-live/solo-middle-continuation'
    if retired:
        folder = ROOT / 'artifacts/geomod-postedit-re/detached-retirement-restart'
    if support:
        folder = ROOT / 'artifacts/geomod-postedit-re/detached-support-restart'
    if args.output_dir is not None:
        folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'artifacts/authored-post-live' /
              ('reset-recut.bin' if reset else 'two-shot.bin')).read_bytes()
    frames, split, initial_cuts, final_cuts = (840, 640, 0, 1) if reset else (550, 350, 1, 2)
    assert source[:8] == b'RFI6' + struct.pack('<I', 48) and len(source) == 8 + frames * 48
    if middle:
        from replay_authored_post import pitch_commands, pitch_for
        recipe = json.loads((ROOT / 'artifacts/authored-post-live/post-recipe.json').read_text())
        pitch, _ = pitch_commands(0, pitch_for(recipe['eye'], [-4.699, .25, 2.5]))
        source = bytearray(source)
        for frame in range(frames):
            struct.pack_into('<f', source, 8 + frame * 48 + 12, pitch[frame-190] if 190 <= frame < 220 else 0)
            struct.pack_into('<I', source, 8 + frame * 48 + 32, int(frame == 240))
        source = bytes(source)
        final_cuts = 1
    if retired or support:
        source = (ROOT / ('artifacts/geomod-postedit-re/detached-support/inputs.bin' if support else 'artifacts/geomod-postedit-re/detached-rocket/inputs.bin')).read_bytes()
        split = (len(source)-8)//48
        source += bytes(200*48)
        frames = split+200
        initial_cuts = final_cuts = 2 if support else 1
    recordings = {'saved': source[:8 + split * 48],
                  'continued': source[:8] + source[8 + split * 48:], 'control': source}
    env = {k: v for k, v in os.environ.items() if not k.startswith(("RF_REPLAY_", "RF_DEV_"))}
    env.update(RF_REPLAY_LEVEL="ctf06.rfl", RF_REPLAY_ARCHIVE="levelsm.vpp",
               RF_REPLAY_DEV_ROOM="1", RF_REPLAY_PLAYER_CHECKPOINT="1")
    report = {"result": "FAIL", "case": args.case, "scope": "PC save then next blast versus uninterrupted run; no visual or Xbox acceptance"}
    try:
        for name, recording in recordings.items():
            path = folder / name
            path.with_suffix(".bin").write_bytes(recording)
            output = path.with_suffix(".rfcp")
            output.unlink(missing_ok=True)
            local = dict(env, RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(output),
                         RF_REPLAY_AUTHORED_HISTORY_OUT=str(path.with_suffix(".rgch")),
                         RF_REPLAY_AUTHORED_PUBLICATION_OUT=str(path.with_suffix(".rgp")))
            if name == "continued":
                local["RF_REPLAY_GEOMOD_CHECKPOINT_IN"] = str(folder / "saved.rfcp")
            with path.with_suffix(".log").open("wb") as log:
                result = subprocess.run([str(args.build_dir.resolve() / "Release/rf_pc_play.exe"), "--spawn-replay",
                                         str(ROOT / "Installed_Game"), str(path.with_suffix(".bin")),
                                         str(path.with_suffix(".ppm"))], cwd=ROOT, env=local,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=180)
            assert result.returncode == 0 and output.exists(), name + " failed; inspect log"
        for extension in ("rfcp", "rgch", "rgp"):
            a = (folder / ("continued." + extension)).read_bytes()
            b = (folder / ("control." + extension)).read_bytes()
            assert a == b, extension + " differs after restart continuation"
            report[extension] = {"bytes": len(a), "sha256": hashlib.sha256(a).hexdigest()}
        for name, cuts in (("saved", initial_cuts), ("continued", final_cuts), ("control", final_cuts)):
            data = (folder / (name + ".rgch")).read_bytes()
            assert struct.unpack_from("<I", data, 12)[0] == cuts, name + " did not produce expected destruction"
        if retired:
            for name in recordings:
                lines = (folder / (name + '.log')).read_text().splitlines()
                pieces = list(map(int, next(x for x in lines if x.startswith('DETACHED_PIECES ')).split()[1:]))
                motion = list(map(int, next(x for x in lines if x.startswith('DETACHED_MOTION ')).split()[1:]))
                assert pieces[:4] == [1,1,0,0] and pieces[5] == 0, pieces
                assert motion == [0]*8, motion
                assert 0 < pieces[4] <= 2*1024*1024
            checkpoint = (folder / 'continued.rfcp').read_bytes()
            piece_bytes = struct.unpack_from('<I', checkpoint, 576+12)[0]
            trailer = checkpoint[-piece_bytes:]
            assert trailer[:4] == b'RFPB' and struct.unpack_from('<3I',trailer,4) == (2,344,1)
            health, flags = struct.unpack_from('<fI',trailer,16+320)
            assert health < 0 and flags == 0x6200002
            report['piece_health'] = health
            report['piece_flags'] = flags
            report['post_image'] = check_post_image(folder)
            report['scope'] = 'Real two-rocket chunk retirement, absent draw/motion, exact saved continuation and fixed-camera post pixels; no Xbox or audio acceptance'
        if middle or support:
            states = {}
            for name in recordings:
                lines = (folder / (name + '.log')).read_text().splitlines()
                state = list(map(int, next(line for line in lines if line.startswith('DETACHED_PIECES ')).split()[1:]))
                assert state[0:3] == [1, 1, 1] and state[3] > 0 and state[5] == 0, name + ' missing detached piece'
                assert 0 < state[4] <= 2 * 1024 * 1024, name + ' detached budget'
                states[name] = state
            assert states['continued'] == states['control'], 'Detached ownership/draw differs after reload'
            motion = {}
            for name in recordings:
                lines = (folder / (name + '.log')).read_text().splitlines()
                values = list(map(int, next(line for line in lines if line.startswith('DETACHED_MOTION ')).split()[1:]))
                pose = list(map(float, next(line for line in lines if line.startswith('DETACHED_POSE ')).split()[1:]))
                assert values[0] == 1 and values[4] == 0 and values[6] == 0 and values[7] == 60
                motion[name] = dict(state=values, pose=pose)
            assert motion['continued'] == motion['control']
            assert motion['control']['state'][3] == 1 and motion['control']['pose'][1] < 0
            report['motion'] = motion
            report['detached'] = states
            report['post_image'] = check_post_image(folder)
            report['scope'] = 'PC real rocket separation, falling/settled state and checkpoint continuation; fixed-camera post-region parity; no Xbox or audio acceptance'
        report["result"] = "PASS"
    finally:
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
