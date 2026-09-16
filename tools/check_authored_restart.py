"""Process-local authored post checkpoint continuation; no host input or emulator."""
import argparse
import struct
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=('two-shot', 'reset-zero'), default='two-shot')
    args = parser.parse_args()
    reset = args.case == 'reset-zero'
    folder = ROOT / ('artifacts/authored-post-live/solo-reset-continuation' if reset else
                     'artifacts/authored-post-live/solo-continuation')
    folder.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'artifacts/authored-post-live' /
              ('reset-recut.bin' if reset else 'two-shot.bin')).read_bytes()
    frames, split, initial_cuts, final_cuts = (840, 640, 0, 1) if reset else (550, 350, 1, 2)
    assert source[:8] == b'RFI6' + struct.pack('<I', 48) and len(source) == 8 + frames * 48
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
                result = subprocess.run([str(ROOT / "build/pc/Release/rf_pc_play.exe"), "--spawn-replay",
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
        report["result"] = "PASS"
    finally:
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
