"""Image-free inactive and live active-cutscene checkpoint round trips."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from check_ordinary_save_reload import payload, sections

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build/pc/Release/rf_pc_play.exe"
GAME = ROOT / "Installed_Game"


def replay(folder: Path, level: str, frames: int, setup: int | None, extra: dict[str, str],
           archive: str = "levels3.vpp") -> str:
    inputs = folder / "neutral.bin"
    inputs.write_bytes(b"RFI6" + struct.pack("<I", 48) + bytes(frames * 48))
    env = os.environ.copy()
    for name in ("RF_REPLAY_SETUP_UID", "RF_REPLAY_WORLD_SNAPSHOT_IN", "RF_REPLAY_CAPTURE_DIR"):
        env.pop(name, None)
    env.update(RF_REPLAY_LEVEL=level, RF_REPLAY_ARCHIVE=archive, **extra)
    if setup is not None:
        env["RF_REPLAY_SETUP_UID"] = str(setup)
    run = subprocess.run(
        [str(EXE), "--spawn-telemetry-replay", str(GAME), str(inputs)],
        cwd=folder, env=env, capture_output=True, text=True, timeout=120,
    )
    if run.returncode:
        raise AssertionError(f"{level} replay failed ({run.returncode}):\n{run.stderr}\n{run.stdout[-2500:]}")
    return run.stdout


def cutscene(log: str) -> list[int]:
    rows = [list(map(int, line.split()[1:])) for line in log.splitlines() if line.startswith("CUTSCENE ")]
    assert rows, "CUTSCENE telemetry missing"
    return rows[-1]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="rf-cutscene-save-") as temp:
        folder = Path(temp) / "L1S1"
        folder.mkdir()
        saved = replay(folder, "L1S1.rfl", 90, None,
                       {"RF_REPLAY_QUICKSAVE_FRAME": "60"}, "levels1.vpp")
        assert re.search(r"QUICK_SAVE frame60 status0\b", saved), "\n".join(
            line for line in saved.splitlines() if "SNAPSHOT" in line or "QUICK_SAVE" in line
        )
        path = folder / "redfaction-save"
        assert list(folder.glob("redfaction-save*")), "save file missing"
        loaded = replay(folder, "L1S1.rfl", 2, None,
                        {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)}, "levels1.vpp")
        assert "WORLD_SNAPSHOT_LOADED " in loaded, loaded[-2500:]
        assert cutscene(loaded)[7] == 0, "inactive checkpoint restarted a cutscene"
        print("PASS L1S1: RFEN3 inactive cutscene round trip")

        folder = Path(temp) / "L17S4"
        folder.mkdir()
        active = replay(folder, "L17S4.rfl", 65, None,
                        {"RF_REPLAY_QUICKSAVE_FRAME": "60"})
        assert cutscene(active)[7] == 1, "cutscene fixture did not start"
        assert "QUICK_SAVE frame60 status0" in active, "\n".join(
            line for line in active.splitlines() if "SNAPSHOT" in line or "QUICK_SAVE" in line
        )
        path = folder / "redfaction-save"
        assert list(folder.glob("redfaction-save*")), "active save file missing"
        packed = sections(payload(path))
        environment = packed["environment"]
        assert environment[:4] == b"RFEN" and struct.unpack_from("<I", environment, 4)[0] == 3
        assert environment[-96:-92] == b"RFCC"
        assert struct.unpack_from("<II", environment, len(environment)-88) == (1, 18248)
        events = packed["events"]
        pending = [struct.unpack_from("<i", events, i+160)[0]
                   for i in range(0, len(events), 192)
                   if struct.unpack_from("<II", events, i+16) == (20698, 61)]
        assert len(pending) == 1 and pending[0] > 16000, pending
        resumed = replay(folder, "L17S4.rfl", 2, None,
                         {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)})
        assert "WORLD_SNAPSHOT_LOADED " in resumed, resumed[-2500:]
        state = cutscene(resumed)
        assert state[3] == 18248 and state[7] == 1 and state[9] == 0, state
        continued = replay(folder, "L17S4.rfl", 1200, None,
                           {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)})
        assert continued.count("WORLD_SNAPSHOT_LOADED ") == 1, "snapshot reapplied after level handoff"
        handoff = re.search(r"LEVEL_TRANSITION L17S4\.rfl L18S1\.rfl 18265 (\d+)", continued)
        assert handoff and 1060 <= int(handoff.group(1)) < 1200, "authored handoff missing or early"
        assert "LEVEL_ARRIVAL " in continued and "WORLD_SNAPSHOT_LOAD_REJECT" not in continued
        print("PASS L17S4: active timeline and queued blackout restore through L18S1 handoff")

        folder = Path(temp) / "L14S3"
        folder.mkdir()
        active = replay(folder, "L14S3.rfl", 65, 9618,
                        {"RF_REPLAY_QUICKSAVE_FRAME": "60"})
        assert cutscene(active)[3] == 9618 and "QUICK_SAVE frame60 status0" in active, active[-2500:]
        path = folder / "redfaction-save"
        environment = sections(payload(path))["environment"]
        assert struct.unpack_from("<I", environment, 76)[0] == 6711, "support mover UID missing"
        resumed = replay(folder, "L14S3.rfl", 120, None,
                         {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)})
        assert "WORLD_SNAPSHOT_LOADED " in resumed, resumed[-2500:]
        assert re.search(r"WORLD_PLAYER_SUPPORT_RESTORED uid6711 handle\d+ velocity", resumed)
        state = cutscene(resumed)
        assert state[3] == 9618 and state[7] == 1 and state[9] == 0, state
        direct = replay(folder, "L14S3.rfl", 180, 9618, {})
        body = lambda log: [line for line in log.splitlines() if line.startswith("PC_PLAY_BODY ")][-1]
        assert body(resumed) == body(direct), "restored player diverged from uninterrupted mover support"
        print("PASS L14S3: active cutscene and authored mover support survive ordinary reload")

        folder = Path(temp) / "L11S3"
        folder.mkdir()
        active = replay(folder, "L11S3.rfl", 65, 10626,
                        {"RF_REPLAY_QUICKSAVE_FRAME": "60"}, "levels2.vpp")
        assert cutscene(active)[3] == 10626 and "QUICK_SAVE frame60 status0" in active, active[-2500:]
        path = folder / "redfaction-save"
        resumed = replay(folder, "L11S3.rfl", 120, None,
                         {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)}, "levels2.vpp")
        assert "WORLD_SNAPSHOT_LOADED " in resumed and cutscene(resumed)[3] == 10626, resumed[-2500:]
        direct = replay(folder, "L11S3.rfl", 180, 10626, {}, "levels2.vpp")
        assert body(resumed) == body(direct), "restored L11S3 player diverged from uninterrupted play"
        print("PASS L11S3: active cutscene, authored mover/NPC overlap and player start prop survive reload")

        folder = Path(temp) / "L6S3"
        folder.mkdir()
        active = replay(folder, "L6S3.rfl", 65, 3696,
                        {"RF_REPLAY_QUICKSAVE_FRAME": "60"}, "levels1.vpp")
        assert cutscene(active)[3] == 3696 and "QUICK_SAVE frame60 status0" in active, active[-2500:]
        path = folder / "redfaction-save"
        assert Path(str(path) + ".0").exists(), "L6S3 active checkpoint missing"
        resumed = replay(folder, "L6S3.rfl", 120, None,
                         {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(path)}, "levels1.vpp")
        assert "WORLD_SNAPSHOT_LOADED " in resumed and cutscene(resumed)[3] == 3696, resumed[-2500:]
        direct = replay(folder, "L6S3.rfl", 180, 3696, {}, "levels1.vpp")
        assert body(resumed) == body(direct), "restored L6S3 player diverged from uninterrupted play"
        assert cutscene(resumed)[7] == cutscene(direct)[7] == 1
        print("PASS L6S3: queued visual monitor refresh, clustered NPCs and active timeline reload")


if __name__ == "__main__":
    main()
