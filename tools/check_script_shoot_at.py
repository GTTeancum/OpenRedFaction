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
        # Stage the player by the linked shooter. The authored L15S1 firing
        # line crosses that position; the ordinary damage path must handle it.
        env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
        env.update(RF_REPLAY_LEVEL="L15S1.rfl", RF_REPLAY_ARCHIVE="levels3.vpp",
                   RF_REPLAY_ACTOR_UID="8278", RF_REPLAY_GOTO_UID="9489", RF_REPLAY_GOTO_FRAME="30")
        run = subprocess.run((str(EXE), "--spawn-telemetry-replay", str(GAME), str(replay)),
                             cwd=ROOT, env=env, text=True, capture_output=True, check=True)
        shoot = words(run.stdout, "SCRIPT_SHOOT_AT")
        combat = words(run.stdout, "ENEMY_COMBAT")
        print("L15S1.rfl staged 8278", "script_shots", shoot[3],
              "incidental_damage_hits", shoot[7], "all_damage_hits", combat[3])
        if shoot[3] == 0 or shoot[7] == 0 or combat[3] < shoot[7]:
            raise RuntimeError("Authored Shoot_At firing line missed the staged player")


if __name__ == "__main__":
    main()
