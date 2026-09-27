"""Text-only, process-contained PC pickup and weapon-switch smoke check."""

import os
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Installed_Game"
PLAYER = ROOT / "build/pc/Release/rf_pc_play.exe"


def run_case(name, inputs, settings):
    with tempfile.TemporaryDirectory(prefix="rf-weapon-check-") as directory:
        replay = Path(directory) / "input.bin"
        replay.write_bytes(
            b"RFI5" + struct.pack("<I", 44)
            + b"".join(struct.pack("<5f6I", *row) for row in inputs)
        )
        env = {key: value for key, value in os.environ.items()
               if not key.startswith("RF_REPLAY_")}
        env.update(settings)
        result = subprocess.run(
            [str(PLAYER), "--spawn-telemetry-replay", str(GAME), str(replay)],
            cwd=ROOT, env=env, capture_output=True, text=True, check=False,
        )
    if result.returncode:
        raise AssertionError(f"{name}: exit {result.returncode}: {result.stderr[-1000:]}")
    rows = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if fields and fields[0] in ("PICKUPS", "PLAYER_AMMO", "WEAPON_SELECTION"):
            rows[fields[0]] = [int(value) for value in fields[1:]]
    if len(rows) != 3:
        raise AssertionError(f"{name}: missing telemetry {rows.keys()}")
    return rows


def input_row(move=0, cycle=0):
    # move xyz, look pitch/yaw, crouch/jump/use/fire/reload/cycle.
    return (0., 0., float(move), 0., 0., 0, 0, 0, 0, 0, cycle)


def main():
    pickup = run_case(
        "L1S1 pickup",
        [input_row(move=130 <= frame < 145, cycle=int(frame in (180, 200)))
         for frame in range(240)],
        {"RF_REPLAY_ITEM_UID": "9427", "RF_REPLAY_LEVEL": "L1S1.rfl",
         "RF_REPLAY_ARCHIVE": "levels1.vpp"},
    )
    assert pickup["PICKUPS"][3:6] == [1, 16, 9427], pickup
    assert pickup["PLAYER_AMMO"][0] != 0xFFFFFFFF, pickup
    assert pickup["PLAYER_AMMO"][2] == 16, pickup
    # Only one owned weapon: cycling should leave that weapon selected.
    assert pickup["WEAPON_SELECTION"][1] == 0, pickup

    switches = run_case(
        "developer-room cycling",
        [input_row(cycle=1 if frame in (20, 21, 22, 40, 60, 80, 100, 120)
                   else 2 if frame == 140 else 0)
         for frame in range(180)],
        {"RF_REPLAY_DEV_ROOM": "1", "RF_REPLAY_LEVEL": "glass_house.rfl",
         "RF_REPLAY_ARCHIVE": "levelsm.vpp"},
    )
    # Held input is one edge; five more forward presses and one backward press.
    assert switches["WEAPON_SELECTION"][1] == 7, switches
    assert switches["WEAPON_SELECTION"][0] == 5, switches
    assert switches["PLAYER_AMMO"][0] != pickup["PLAYER_AMMO"][0], switches
    print("PASS: authored handgun pickup/equip; owned-only forward/backward weapon cycling")
    print("Interactive keyboard, XInput and visible presentation remain unverified.")


if __name__ == "__main__":
    main()
