"""Image-free authored undercover OFF and section-carry smoke checks on PC.

The source is a process-local neutral replay. It neither sends host input nor
captures a framebuffer; the Xbox equivalent uses xemu_render_check.py with
--no-images and the same authored UIDs.
"""
import os
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Installed_Game"
PLAYER = ROOT / "build/pc/Release/rf_pc_play.exe"


def run_case(path, name, frames, level, archive, setup, exit_uid=None, return_uid=None):
    replay = path / (name + ".bin")
    replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + bytes(frames * 48))
    env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_ARCHIVE=archive,
               RF_REPLAY_SETUP_UID=setup)
    if exit_uid is not None:
        env["RF_REPLAY_EXIT_UID"] = str(exit_uid)
    if return_uid is not None:
        env["RF_REPLAY_RETURN_EXIT_UID"] = str(return_uid)
    result = subprocess.run([str(PLAYER), "--spawn-telemetry-replay", str(GAME), str(replay)],
                            cwd=ROOT, env=env, capture_output=True, text=True, check=True)
    lines = result.stdout.splitlines()
    assert f"Completed {frames} frames" in result.stdout, name
    return lines


def one(lines, prefix):
    values = [line for line in lines if line.startswith(prefix + " ")]
    assert len(values) == 1, (prefix, values)
    return values[0]


def main():
    with tempfile.TemporaryDirectory(prefix="undercover-", dir=ROOT / "artifacts") as temp:
        path = Path(temp)
        off = run_case(path, "off", 240, "L6S3.rfl", "levels1.vpp", "6938,6939")
        form = one(off, "PLAYER_FORM")
        assert form == "PLAYER_FORM 0 0 0 1 1 4 1120403456 0", form
        assert one(off, "PLAYER_AMMO").split()[1] != "4", "normal weapon not restored"
        carried = run_case(path, "carry", 240, "L8S1.rfl", "levels2.vpp", "6447", 5625, 5623)
        transitions = [line for line in carried if line.startswith("LEVEL_TRANSITION ")]
        assert transitions == ["LEVEL_TRANSITION L8S1.rfl L8S2.rfl 5625 61",
                               "LEVEL_TRANSITION L8S2.rfl L8S1.rfl 5623 181"], transitions
        assert one(carried, "PLAYER_FORM") == "PLAYER_FORM 1 1 0 0 0 4 0 0"
        assert one(carried, "PLAYER_AMMO") == "PLAYER_AMMO 4 125 16 0 0 0 448 0"
    print("PASS: authored OFF restores normal form; scientist form and handgun survive both section exits")


if __name__ == "__main__":
    main()
