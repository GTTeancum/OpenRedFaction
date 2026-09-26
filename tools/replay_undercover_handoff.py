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


def run_case(path, name, frames, level, archive, setup, exit_uid=None, return_uid=None,
             walk_start_uid=None, walk_from=30):
    replay = path / (name + ".bin")
    inputs = (b"".join(struct.pack("<5f7I", 0, 0, float(frame >= walk_from), 0, 0,
                                   0, 0, 0, 0, 0, 0, 0)
                       for frame in range(frames)) if walk_start_uid else bytes(frames * 48))
    replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + inputs)
    env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_ARCHIVE=archive,
               RF_REPLAY_SETUP_UID=setup)
    if exit_uid is not None:
        env["RF_REPLAY_EXIT_UID"] = str(exit_uid)
    if return_uid is not None:
        env["RF_REPLAY_RETURN_EXIT_UID"] = str(return_uid)
    if walk_start_uid is not None:
        env["RF_REPLAY_EXIT_START"] = str(walk_start_uid)
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
        assert one(off, "PLAYER_MODEL") == "PLAYER_MODEL 0 88108 25 2"
        natural_off = run_case(path, "natural-off", 300, "L6S3.rfl", "levels1.vpp",
                               "6938", walk_start_uid=6939, walk_from=90)
        assert one(natural_off, "PLAYER_FORM") == "PLAYER_FORM 0 0 0 1 1 4 1120403456 0"
        assert one(natural_off, "PLAYER_MODEL") == "PLAYER_MODEL 0 88108 25 2"
        assert one(natural_off, "PLAYER_AMMO") == "PLAYER_AMMO 3 125 16 0 0 0 448 0"
        contacts = one(natural_off, "TRIGGER_CONTACTS").split()
        assert int(contacts[2]) > 0 and contacts[3] == "6937", contacts
        entered = run_case(path, "entered", 90, "L8S1.rfl", "levels2.vpp", "6447")
        assert one(entered, "PLAYER_FORM") == "PLAYER_FORM 1 1 0 1 0 4 0 0"
        assert one(entered, "PLAYER_MODEL") == "PLAYER_MODEL 2 80555 25 1"
        carried = run_case(path, "carry", 240, "L8S1.rfl", "levels2.vpp", "6447", 5625, 5623)
        transitions = [line for line in carried if line.startswith("LEVEL_TRANSITION ")]
        assert transitions == ["LEVEL_TRANSITION L8S1.rfl L8S2.rfl 5625 61",
                               "LEVEL_TRANSITION L8S2.rfl L8S1.rfl 5623 181"], transitions
        assert one(carried, "PLAYER_FORM") == "PLAYER_FORM 1 1 0 0 0 4 0 0"
        assert one(carried, "PLAYER_AMMO") == "PLAYER_AMMO 4 125 16 0 0 0 448 0"
        assert one(carried, "PLAYER_MODEL") == "PLAYER_MODEL 2 80555 25 0"
        walked = run_case(path, "walked", 240, "L8S1.rfl", "levels2.vpp", "6447",
                          walk_start_uid=5625)
        transitions = [line.split() for line in walked if line.startswith("LEVEL_TRANSITION ")]
        assert len(transitions) == 1 and transitions[0][1:4] == [
            "L8S1.rfl", "L8S2.rfl", "5625"] and 30 < int(transitions[0][4]) < 120, transitions
        assert one(walked, "PLAYER_FORM") == "PLAYER_FORM 1 1 0 0 0 4 0 0"
        assert one(walked, "PLAYER_AMMO") == "PLAYER_AMMO 4 125 16 0 0 0 448 0"
        assert one(walked, "PLAYER_MODEL") == "PLAYER_MODEL 2 80555 25 0"
        suit = run_case(path, "suit", 240, "L6S2.rfl", "levels1.vpp", "2195", 1782)
        assert [line for line in suit if line.startswith("LEVEL_TRANSITION ")] == [
            "LEVEL_TRANSITION L6S2.rfl L6S3.rfl 1782 180"]
        assert one(suit, "PLAYER_FORM") == "PLAYER_FORM 1 0 0 0 0 4 0 0"
        assert one(suit, "PLAYER_MODEL") == "PLAYER_MODEL 1 77931 25 0"
    print("PASS: authored ON/OFF, natural suit removal, both forms carry, scientist walks through an exit")


if __name__ == "__main__":
    main()
