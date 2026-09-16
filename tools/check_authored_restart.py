"""Process-local authored post checkpoint continuation; no host input or emulator."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    folder = ROOT / "artifacts/authored-post-live/solo-continuation"
    folder.mkdir(parents=True, exist_ok=True)
    source = (ROOT / "artifacts/authored-post-live/two-shot.bin").read_bytes()
    assert source[:8] == b"RFI6\x30\x00\x00\x00" and len(source) == 8 + 550 * 48
    recordings = {"saved": source[:8 + 350 * 48],
                  "continued": source[:8] + source[8 + 350 * 48:], "control": source}
    env = {k: v for k, v in os.environ.items() if not k.startswith(("RF_REPLAY_", "RF_DEV_"))}
    env.update(RF_REPLAY_LEVEL="ctf06.rfl", RF_REPLAY_ARCHIVE="levelsm.vpp",
               RF_REPLAY_DEV_ROOM="1", RF_REPLAY_PLAYER_CHECKPOINT="1")
    report = {"result": "FAIL", "scope": "PC one-cut save then second blast versus uninterrupted two-cut run; no visual or Xbox acceptance"}
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
        import struct
        for name, cuts in (("saved", 1), ("continued", 2), ("control", 2)):
            data = (folder / (name + ".rgch")).read_bytes()
            assert struct.unpack_from("<I", data, 12)[0] == cuts, name + " did not produce expected destruction"
        report["result"] = "PASS"
    finally:
        (folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
