"""Check an authored guard's response to loud and suppressed undercover shots.

The replay is process-local and telemetry-only. It does not operate the desktop
or create images. L8S1's Alarm 6449 links the authored armed actor 6467, who is
used here solely as a nearby guard; the alarm event itself is not dispatched.
"""
import os
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Installed_Game"
PLAYER = ROOT / "build/pc/Release/rf_pc_play.exe"
FRAMES = 240


def case(directory, name, suppressed=False, fire=False):
    replay = directory / (name + ".bin")
    replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + b"".join(
        struct.pack("<5f7I", 0, 0, 0, 0, 0, 0, 0, 0,
                    int(fire and frame == 170), 0, 0,
                    int(suppressed and frame == 20))
        for frame in range(FRAMES)))
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL="L8S1.rfl", RF_REPLAY_ARCHIVE="levels2.vpp",
               RF_REPLAY_SETUP_UID="6447", RF_REPLAY_ACTOR_UID="6467")
    result = subprocess.run(
        [str(PLAYER), "--spawn-telemetry-replay", str(GAME), str(replay)],
        cwd=ROOT, env=env, capture_output=True, text=True, check=True)
    assert f"Completed {FRAMES} frames" in result.stdout, name
    return {line.split(" ", 1)[0]: line for line in result.stdout.splitlines()
            if line.startswith(("PLAYER_FORM ", "UNDERCOVER ",
                                "ENEMY_COMBAT ", "COMBAT ", "PLAYER_AMMO "))}


def main():
    with tempfile.TemporaryDirectory(prefix="undercover-guard-", dir=ROOT / "artifacts") as temp:
        directory = Path(temp)
        quiet = case(directory, "quiet")
        loud = case(directory, "loud", fire=True)
        suppressed = case(directory, "suppressed", suppressed=True, fire=True)
    assert quiet["PLAYER_FORM"].split()[1:4] == ["1", "1", "0"], quiet
    assert quiet["COMBAT"].split()[1] == "0", quiet
    assert loud["PLAYER_FORM"].split()[1:4] == ["1", "1", "1"], loud
    assert loud["COMBAT"].split()[1] == "1", loud
    assert int(loud["ENEMY_COMBAT"].split()[3]) > 0, loud
    assert suppressed["PLAYER_FORM"].split()[1:4] == ["1", "1", "0"], suppressed
    assert suppressed["UNDERCOVER"].split()[1:7] == ["1", "0", "1", "1", "1", "1"], suppressed
    assert suppressed["COMBAT"].split()[1] == "1", suppressed
    assert suppressed["ENEMY_COMBAT"].split()[1:5] == ["240", "0", "0", "0"], suppressed
    print("PASS: nearby authored guard detects loud shot but not attached-suppressor shot")


if __name__ == "__main__":
    main()
