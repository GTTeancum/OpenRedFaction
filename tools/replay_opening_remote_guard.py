"""Defeat L1S1 corner guard 8462 from the real post-wall ordinary save.

Process-local replay only. No frame capture or host input.
Requires wall-save/sight-phase.0 and wall-save/wall-cross-save.bin from the
opening save fixtures.
"""

import os
from pathlib import Path
import struct
import subprocess

from capture_replay_aim import capture


root = Path(__file__).resolve().parents[1]
folder = root / "artifacts/opening-exit"
save = folder / "wall-save/sight-phase"
assert (folder / "wall-save/sight-phase.0").is_file()
frames = 151
records = []
for frame in range(frames):
    move_forward = 1.0 if 1 <= frame < 20 else -1.0 if 47 <= frame < 105 else 0.0
    records.append(struct.pack("<5f7I", 0, 0, move_forward, 0, 0,
                               0, 0, 0, int(frame in (25, 115)), 0,
                               2 if frame == 1 else 1 if frame == 55 else 0, 0))
replay = folder / "remote-guard-route.bin"
replay.write_bytes(b"RFI6" + struct.pack("<I", 48) + b"".join(records))

env = {key: value for key, value in os.environ.items()
       if not key.startswith("RF_REPLAY_")}
env.update(RF_REPLAY_LEVEL="L1S1.rfl", RF_REPLAY_ARCHIVE="levels1.vpp",
           RF_REPLAY_WORLD_SNAPSHOT_IN=str(save),
           RF_REPLAY_AIM="8462:1:46", RF_REPLAY_TRACE="1",
           RF_REPLAY_TRACE_FROM="0")
run = subprocess.run([str(root / "build/pc/Release/rf_pc_play.exe"),
                      "--spawn-telemetry-replay", str(root / "Installed_Game"),
                      str(replay)], cwd=root, env=env, capture_output=True, text=True)
output = run.stdout + run.stderr
(folder / "remote-guard-route.log").write_text(output)
run.check_returncode()
rows = output.splitlines()


def row(prefix):
    return next(line for line in rows if line.startswith(prefix))


guard = row("NPC_COMBAT_ROW 8462 ").split()
life = row("PLAYER_LIFE ").split()
remote = row("REMOTE ").split()
assert float(guard[-1]) <= 0 and life[1] == "0"
assert [int(value) for value in remote[1:5]] == [1, 1, 1, 1]
assert "REMOTE_ATTACH 60 " in output and "REMOTE_DETONATE 116 " in output
print("PASS corner guard8462 dead; player alive; remote attach/detonate")
for prefix in ("CAMPAIGN_FINAL_POSITION ", "PICKUP_VITALS ", "PLAYER_AMMO ",
               "WEAPON_SELECTION "):
    print(row(prefix))

baked, aim_frames = capture(replay.read_bytes(), output)
assert aim_frames == 45
(folder / "remote-guard-baked.bin").write_bytes(baked)
spawn_prefix = (folder / "wall-save/wall-cross-save.bin").read_bytes()
assert spawn_prefix[:8] == b"RFI6" + struct.pack("<I", 48)
assert len(spawn_prefix) == 8 + 4335 * 48
(folder / "remote-guard-full.bin").write_bytes(spawn_prefix + baked[8:])
print("Baked 45 look inputs into a 4486-frame spawn route")
