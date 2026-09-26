"""Exercise authored live camera targets without creating any image files."""
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/pc/Release/rf_pc_play.exe"
GAME = ROOT / "Installed_Game"
CASES = (
    ("L14S3.rfl", 9618, 2, 9642, 0, 0),
    ("L14S3.rfl", 9618, 980, 10669, 0, 4),
    ("L17S4.rfl", None, 2, 18250, 2, 0),
)


def main():
    with tempfile.TemporaryDirectory(prefix="rf-cutscene-look-") as folder:
        for level, setup, frames, target, fallback, point in CASES:
            replay = Path(folder) / "neutral.bin"
            replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + bytes(frames * 48))
            env = os.environ.copy()
            env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_ARCHIVE="levels3.vpp")
            env.pop("RF_REPLAY_SETUP_UID", None)
            env.pop("RF_REPLAY_CAPTURE_DIR", None)
            env.pop("RF_REPLAY_DEPTH_OUT", None)
            env.pop("RF_REPLAY_MESH_OUT", None)
            if setup is not None:
                env["RF_REPLAY_SETUP_UID"] = str(setup)
            run = subprocess.run(
                [str(EXE), "--spawn-telemetry-replay", str(GAME), str(replay)],
                cwd=ROOT, env=env, capture_output=True, text=True, timeout=120,
            )
            if run.returncode:
                raise AssertionError(f"{level} replay failed ({run.returncode}):\n{run.stderr}\n{run.stdout[-2000:]}")
            rows = {parts[0]: list(map(int, parts[1:])) for line in run.stdout.splitlines()
                    if (parts := line.split()) and parts[0] in {"CUTSCENE", "CUTSCENE_LOOK"}}
            cut, look = rows["CUTSCENE"], rows["CUTSCENE_LOOK"]
            assert cut[2] == 1 and cut[4] == point and cut[9] == 0, (level, cut)
            assert look[0] > 0 and look[1] == 0 and look[2] == target and look[4] == 0 and look[5] == fallback, (level, look)
            print(f"PASS {level} point {point} target {target}: {look[0]} aims, {look[5]} authored fallbacks")


if __name__ == "__main__":
    main()
