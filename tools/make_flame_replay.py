"""Generate process-local flame input only; never launch or control an app.

Quiet ctf06 DEV NPC2 staging, using the established scanner-facing yaw.
10 forward ticks request roughly0.8m at5units/s; collision/acceleration may
change actual travel. Native output/log inspection must establish hits.
"""
import argparse
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/flame-live/flame.bin")
    parser.add_argument("--frames", type=int, default=300)
    args = parser.parse_args()
    if not 250 <= args.frames < 600:
        parser.error("Use250..599frames; default300 captures after the primary hold.")
    rows = []
    for frame in range(args.frames):
        cycle = frame in range(10, 101, 10)
        yaw = 1.0 if 80 <= frame < 188 else 0.0
        forward = 1.0 if 200 <= frame < 210 else 0.0
        primary = 230 <= frame < 280
        rows.append(struct.pack("<5f7I", 0, 0, forward, 0, yaw,
                                0, 0, 0, int(primary), 0, int(cycle), 0))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(b"RFI6" + struct.pack("<I", 48) + b"".join(rows))
    metadata = {
        "frames": args.frames,
        "env": {
            "RF_REPLAY_LEVEL": "ctf06.rfl", "RF_REPLAY_ARCHIVE": "levelsm.vpp",
            "RF_REPLAY_DEV_ROOM": "1", "RF_REPLAY_DEV_NPC": "2",
            "RF_REPLAY_TRACE": "1", "RF_REPLAY_TRACE_FROM": "0"
        },
        "input": str(args.output.resolve()),
        "command": [str(ROOT / "build/pc/Release/rf_pc_play.exe"), "--spawn-replay",
                    str(ROOT / "Installed_Game"), str(args.output.resolve()),
                    str(args.output.resolve().with_suffix(".ppm"))],
        "intent": "10cycles, established yaw, ten forward ticks, primary230..279; inspect COMBAT_EVENT health/damage and native image.",
        "validation": "Not executed. Gas/pulse counts alone do not prove NPC damage or visual correctness."
    }
    args.output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
