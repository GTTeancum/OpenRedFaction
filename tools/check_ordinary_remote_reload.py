"""Image-free ordinary save/load of a planted charge followed by detonation."""
import os
import re
import struct
import subprocess
from pathlib import Path

from check_remote_pickup import ROOT, prepare, words


def main():
    folder = ROOT / "artifacts/ordinary-remote-save"
    folder.mkdir(parents=True, exist_ok=True)
    prepare(folder, 1)
    replay = folder / "reload-detonate.bin"
    replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + b"".join(
        struct.pack("<5f7I", 0, 0, 0, 0, 0, 0, 0, 0,
                    int(frame in (120, 340)), 0, int(frame in (30, 310)), 0)
        for frame in range(380)))
    for slot in range(2):
        (folder / f"redfaction-save.{slot}").unlink(missing_ok=True)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("RF_REPLAY_", "RF_DEV_"))}
    env.update(RF_REPLAY_LEVEL="ctf06.rfl", RF_REPLAY_ARCHIVE="levelsm.vpp",
               RF_REPLAY_SETUP_UID="910530", RF_REPLAY_QUICKSAVE_FRAME="260",
               RF_REPLAY_QUICKLOAD_FRAME="280")
    with (folder / "run.log").open("w") as log:
        subprocess.run([str(ROOT / "build/pc/Release/rf_pc_play.exe"),
                        "--spawn-telemetry-replay", str(folder / "remote/game"), str(replay)],
                       cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=240)
    output = (folder / "run.log").read_text(errors="replace")
    assert "WORLD_SNAPSHOT_COMPONENT remote 276" in output
    assert "QUICK_SAVE frame260 status0" in output
    assert "QUICK_LOAD frame280 status0" in output
    assert re.search(r"^LEVEL_TRANSITION ctf06\.rfl ctf06\.rfl 4294967293 281$",
                     output, re.MULTILINE)
    assert "WORLD_SNAPSHOT_LOADED bytes17440 " in output
    assert words(output, "REMOTE") == [0, 0, 0, 1, 0, 0, 0, 0]
    assert words(output, "WEAPON_SELECTION")[0] == 9
    assert words(output, "PLAYER_AMMO")[:3] == [0, 2, 2]
    print("PASS: planted remote charge survives ordinary quick-load and detonates")


if __name__ == "__main__":
    main()
