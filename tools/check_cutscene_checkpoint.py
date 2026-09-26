"""Image-free checkpoint round trip and active-cutscene admission boundary."""
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

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
        assert "WORLD_SNAPSHOT_EVENT_PENDING_REJECT uid20698 type61" in active, active[-2500:]
        assert "QUICK_SAVE frame60 status-3" in active, active[-2500:]
        assert not list(folder.glob("redfaction-save*")), "rejected save created a file"
        print("PASS L17S4: active visual event rejects incomplete snapshot cleanly")


if __name__ == "__main__":
    main()
