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
    parser.add_argument('--case', choices=('two-shot', 'reset-zero', 'middle-shot'), default='two-shot')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/pc')
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    reset = args.case == 'reset-zero'
    middle = args.case == 'middle-shot'
    folder = ROOT / ('artifacts/authored-post-live/solo-reset-continuation' if reset else
                     'artifacts/authored-post-live/solo-continuation')
    if middle:
        folder = ROOT / 'artifacts/authored-post-live/solo-middle-continuation'
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
        if middle:
            states = {}
            for name in recordings:
                lines = (folder / (name + '.log')).read_text().splitlines()
                state = list(map(int, next(line for line in lines if line.startswith('DETACHED_PIECES ')).split()[1:]))
                assert state[0:3] == [1, 1, 1] and state[3] > 0 and state[5] == 0, name + ' missing detached piece'
                assert 0 < state[4] <= 2 * 1024 * 1024, name + ' detached budget'
                states[name] = state
            assert states['continued'] == states['control'], 'Detached ownership/draw differs after reload'
            report['detached'] = states
            report['post_image'] = check_post_image(folder)
            report['scope'] = 'PC real rocket separation and birth-pose ownership/drawing across reload; fixed-camera post-region parity only; no motion or Xbox acceptance'
        report["result"] = "PASS"
    finally:
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
