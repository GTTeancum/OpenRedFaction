"""Replay L1S2's authored Driller against a real room-8 wall, without images.

This checks contact provenance, not excavation. It launches only the local PC
game process; input is contained in that process and never reaches the host UI.
"""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess

from inspect_geomod_source_topology import load
from locate_campaign_geomod_surface import owners

ROOT = Path(__file__).resolve().parents[1]
CONTACT = re.compile(r"^DRILL_CUT (\d+) (\d+) (-?\d+) (\d+) ([\deE+.-]+) ([\deE+.-]+) ([\deE+.-]+) (\d+) (\d+)$", re.M)


def input_bytes():
    rows = []
    for frame in range(330):
        turn = .6 if 40 <= frame < 105 else 0.
        drive = 140 <= frame < 280
        fire = 80 <= frame < 300
        rows.append(struct.pack("<5f6I", turn, 0., float(drive), 0., 0.,
                                0, 0, int(frame == 30), int(fire), 0, 0))
    return b"RFI5" + struct.pack("<I", 44) + b"".join(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/driller-campaign-contact")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    inputs = output / "inputs.bin"
    inputs.write_bytes(input_bytes())
    executable = ROOT / "build/pc/Release/rf_pc_play.exe"
    env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL="L1S2.rfl", RF_REPLAY_ARCHIVE="levels1.vpp", RF_REPLAY_ACTOR_UID="8122")
    process = subprocess.run([str(executable), "--spawn-telemetry-replay", str(ROOT / "Installed_Game"),
                              str(inputs)], cwd=ROOT, env=env, capture_output=True, text=True)
    log = process.stdout + process.stderr
    (output / "pc-log.txt").write_text(log)
    process.check_returncode()
    compiled, _ = load("L1S2.rfl")
    by_face = {face["id"]: face for face in compiled}
    by_source, _ = owners("L1S2.rfl")
    contacts = []
    for match in CONTACT.finditer(log):
        frame, bit, status, accepted = map(int, match.group(1, 2, 3, 4))
        point = tuple(map(float, match.group(5, 6, 7)))
        room, face_id = map(int, match.group(8, 9))
        face = by_face[face_id]
        if face["room"] != room:
            raise ValueError("contact room and compiled face disagree")
        source_word = face["source_word"]
        owner = by_source[source_word]
        contacts.append(dict(frame=frame, bit=bit, status=status, accepted=accepted,
                             point=point, room=room, face=face_id, source_word=source_word,
                             owner_uid=owner[0], owner_operation=owner[1]))
    if not contacts or contacts[0]["room"] != 8 or contacts[0]["face"] != 768 or contacts[0]["owner_uid"] != 8123:
        raise AssertionError("authored Driller did not reach the expected first wall contact")
    print(json.dumps(dict(result="PASS", level="L1S2.rfl", actor_uid=8122,
                          contacts=contacts, scope="First-contact owner identity only; cut acceptance, visuals and Xbox remain open")))


if __name__ == "__main__":
    main()
