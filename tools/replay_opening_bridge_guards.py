"""Clear both L1S1 bridge guards through ordinary input and save/reload.

Requires the earlier opening wall/armor fixtures from replay_opening_remote_guard.py
and replay_opening_bridge_cover.py. Uses process-local aim only to record bounded
look input, then verifies a plain replay. No image capture or host input.
"""

import os
from pathlib import Path
import struct
import subprocess

from capture_replay_aim import capture


root = Path(__file__).resolve().parents[1]
folder = root / "artifacts/opening-exit"
header = b"RFI6" + struct.pack("<I", 48)
neutral = struct.pack("<5f7I", *([0] * 12))


def records(name):
    data = (folder / name).read_bytes()
    assert data[:8] == header and (len(data) - 8) % 48 == 0, name
    return data[8:]


def run(name, payload, *, world_in, world_out=None, aim=None, trace=False):
    path = folder / (name + ".bin")
    path.write_bytes(payload)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL="L1S1.rfl", RF_REPLAY_ARCHIVE="levels1.vpp",
               RF_REPLAY_WORLD_SNAPSHOT_IN=str(folder / world_in))
    if world_out:
        env["RF_REPLAY_WORLD_SNAPSHOT_OUT"] = str(folder / world_out)
    if aim:
        env["RF_REPLAY_AIM"] = aim
    if trace:
        env.update(RF_REPLAY_TRACE="1", RF_REPLAY_TRACE_FROM="0")
    result = subprocess.run(
        [str(root / "build/pc/Release/rf_pc_play.exe"),
         "--spawn-telemetry-replay", str(root / "Installed_Game"), str(path)],
        cwd=root, env=env, capture_output=True, text=True)
    output = result.stdout + result.stderr
    (folder / (name + ".log")).write_text(output)
    result.check_returncode()
    return output


def row(output, prefix):
    return next(line for line in output.splitlines() if line.startswith(prefix))


old = records("postwall-bridge-cover.bin")
assert len(old) == 5452 * 48
assert (folder / "remote-guard-after-armor-save.0").is_file()
arrival = header + neutral + old[4459 * 48:5452 * 48]
arrived = run("bridge-armor-arrival", arrival,
              world_in="remote-guard-after-armor-save",
              world_out="bridge-armor-arrival-save")
assert "WORLD_SNAPSHOT_STORED " in arrived
assert row(arrived, "PLAYER_LIFE ").split()[1] == "0"
position = tuple(map(float, row(arrived, "CAMPAIGN_FINAL_POSITION ").split()[1:4]))
assert max(abs(a - b) for a, b in zip(position, (39.694, 10.881, 75.951))) < .15, position


def frame(index):
    x = (-1 if 1 <= index <= 80 or 260 <= index < 330 else
         1 if 152 <= index < 260 or 330 <= index <= 360 else 0)
    fire = int(index in {31, 51, 71, 91, 111, 131, 151, 191, 231,
                         320, 360, 400, 440})
    return struct.pack("<5f7I", x, 0, 0, 0, 0, 0, 0, 0,
                       fire, 0, int(index == 2), 0)


raw = header + b"".join(frame(index) for index in range(470)) + neutral * 100
first = run("bridge-guard-9406-aim", raw,
            world_in="bridge-armor-arrival-save", aim="9406:1:112")
baked, first_count = capture(raw, first)
assert first_count == 111, first_count
second = run("bridge-guard-9404-aim", baked,
             world_in="bridge-armor-arrival-save", aim="9404:112:450")
baked, second_count = capture(baked, second)
assert second_count == 289, second_count
plain = run("bridge-guards-baked", baked,
            world_in="bridge-armor-arrival-save",
            world_out="bridge-guards-save", trace=True)
assert "WORLD_SNAPSHOT_STORED " in plain
assert row(plain, "PLAYER_LIFE ").split()[1] == "0"
for uid in (9404, 9406):
    assert float(row(plain, f"NPC_COMBAT_ROW {uid} ").split()[-1]) <= 0

reload = run("bridge-guards-reload", header + neutral * 4,
             world_in="bridge-guards-save", trace=True)
assert "WORLD_SNAPSHOT_LOADED " in reload
assert row(reload, "PLAYER_LIFE ").split()[1] == "0"
for uid in (9404, 9406):
    assert float(row(reload, f"NPC_COMBAT_ROW {uid} ").split()[-1]) <= 0

spawn = records("remote-guard-full.bin")
assert len(spawn) == 4486 * 48
forward = struct.pack("<5f7I", 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)
full = header + spawn + forward * 80 + old[4301 * 48:4510 * 48] + \
       old[4459 * 48:5452 * 48] + baked[8:]
assert (len(full) - 8) // 48 == 6338
(folder / "opening-both-bridge-guards-full.bin").write_bytes(full)
print("PASS L1S1 guards 9404/9406 defeated, player alive, ordinary save/reload")
print("6338-frame real-spawn PC/Xbox replay prepared; no images")
