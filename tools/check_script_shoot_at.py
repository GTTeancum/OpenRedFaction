"""Image-free authored Shoot_At gameplay replay on the PC build."""
import os
import re
import struct
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Installed_Game"
EXE = ROOT / "build/pc/Release/rf_pc_play.exe"
CASES = (("L13S3.rfl", 8858), ("L14S2.rfl", 10582), ("L15S1.rfl", 9489))


def words(output: str, label: str) -> list[int]:
    match = re.search(rf"^{label} ((?:\d+\s*)+)$", output, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Missing {label} in process-local replay")
    return [int(word) for word in match.group(1).split()]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="rf-shoot-at-") as temporary:
        replay = Path(temporary) / "neutral.bin"
        replay.write_bytes(b"RFI5" + struct.pack("<I", 44) + bytes(240 * 44))
        for level, uid in CASES:
            env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
            env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_ARCHIVE="levels3.vpp",
                       RF_REPLAY_GOTO_UID=str(uid), RF_REPLAY_GOTO_FRAME="30")
            run = subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(replay)),
                                 cwd=ROOT, env=env, text=True, capture_output=True, check=True)
            shoot = words(run.stdout, "SCRIPT_SHOOT_AT")
            combat = words(run.stdout, "ENEMY_COMBAT")
            print(level, uid, "assigned", shoot[2], "shots", shoot[3],
                  "enemy_shots", combat[2], "player_damage", combat[3])
            if shoot[0] != 1 or shoot[4] != uid or shoot[2] == 0:
                raise RuntimeError(f"Authored Shoot_At did not assign its linked actor in {level}")
            if level != "L13S3.rfl" and (shoot[3] == 0 or combat[2] < shoot[3] or combat[3] != 0):
                raise RuntimeError(f"Fixed-point fire or player isolation failed in {level}")
        # Stage the player by the linked shooter. This placement is lethal
        # before the delayed Shoot_At order starts; later rays must ignore the
        # dead body instead of recording damage against it.
        env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
        env.update(RF_REPLAY_LEVEL="L15S1.rfl", RF_REPLAY_ARCHIVE="levels3.vpp",
                   RF_REPLAY_ACTOR_UID="8278", RF_REPLAY_GOTO_UID="9489", RF_REPLAY_GOTO_FRAME="30")
        run = subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(replay)),
                             cwd=ROOT, env=env, text=True, capture_output=True, check=True)
        shoot = words(run.stdout, "SCRIPT_SHOOT_AT")
        combat = words(run.stdout, "ENEMY_COMBAT")
        print("L15S1.rfl staged 8278", "script_shots", shoot[3],
              "script_player_hits_after_death", shoot[7], "player_dead", combat[6])
        if shoot[3] == 0 or shoot[7] != 0 or combat[6] != 1:
            raise RuntimeError("Shoot_At damaged a dead player")
        # Import a process-local high-health player state so the same authored
        # firing line remains live when the delayed order begins. This is a
        # collision/damage fixture, not a claim about retail health balance.
        state = Path(temporary) / "player-state.bin"
        env.pop("RF_REPLAY_GOTO_UID")
        env.pop("RF_REPLAY_GOTO_FRAME")
        env["RF_REPLAY_PLAYER_STATE_OUT"] = str(state)
        first = Path(temporary) / "one.bin"
        first.write_bytes(b"RFI5" + struct.pack("<I", 44) + bytes(44))
        subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(first)),
                       cwd=ROOT, env=env, text=True, capture_output=True, check=True)
        saved = bytearray(state.read_bytes())
        health_offset = struct.calcsize("<64B32i64i")
        if len(saved) != health_offset + 16:
            raise RuntimeError("Unexpected player handoff size")
        struct.pack_into("<f", saved, health_offset, 10000.0)
        state.write_bytes(saved)
        env.pop("RF_REPLAY_PLAYER_STATE_OUT")
        env.update(RF_REPLAY_PLAYER_STATE_IN=str(state), RF_REPLAY_GOTO_UID="9489", RF_REPLAY_GOTO_FRAME="30")
        run = subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(replay)),
                             cwd=ROOT, env=env, text=True, capture_output=True, check=True)
        shoot = words(run.stdout, "SCRIPT_SHOOT_AT")
        combat = words(run.stdout, "ENEMY_COMBAT")
        print("L15S1.rfl live-line fixture", "script_shots", shoot[3],
              "incidental_player_hits", shoot[7], "player_dead", combat[6])
        if shoot[3] == 0 or shoot[7] == 0 or combat[6] != 0 or combat[3] < shoot[7]:
            raise RuntimeError("Living player did not receive scripted firing-line damage")
        off_replay = Path(temporary) / "off.bin"
        off_replay.write_bytes(b"RFI5" + struct.pack("<I", 44) + bytes(360 * 44))
        control_shots = None
        for invert in (False, True):
            env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
            env.update(RF_REPLAY_LEVEL="L15S1.rfl", RF_REPLAY_ARCHIVE="levels3.vpp",
                       RF_REPLAY_ACTOR_UID="8278", RF_REPLAY_SETUP_UID="9489")
            if invert:
                env.update(RF_REPLAY_GOTO_UID="9491", RF_REPLAY_GOTO_FRAME="180")
            run = subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(off_replay)),
                                 cwd=ROOT, env=env, text=True, capture_output=True, check=True)
            shoot = words(run.stdout, "SCRIPT_SHOOT_AT")
            print("L15S1.rfl", "Invert" if invert else "control", "shots", shoot[3], "off", shoot[1])
            if invert:
                if shoot[0] != 1 or shoot[1] != 1 or shoot[3] == 0 or shoot[3] >= control_shots:
                    raise RuntimeError("Authored Invert did not stop Shoot_At fire")
            else:
                control_shots = shoot[3]


if __name__ == "__main__":
    main()
