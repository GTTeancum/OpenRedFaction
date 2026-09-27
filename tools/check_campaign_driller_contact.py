"""Replay L1S2's authored Driller against a real room-8 wall, without images.

Require the first live cut and room publication. Input stays inside the local
PC game process and never reaches the host UI.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
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


def check_world_save(output, inputs):
    run = output / "ordinary-save"
    (run / "build/data").mkdir(parents=True, exist_ok=True)
    for name in ("geomod-template.bin", "driller-single.bin", "driller-double.bin"):
        source, target = ROOT / "build/data" / name, run / "build/data" / name
        if not target.exists() or not os.path.samefile(source, target):
            shutil.copyfile(source, target)
    neutral = run / "neutral.bin"
    neutral.write_bytes(b"RFI5" + struct.pack("<I", 44) + bytes(60 * 44))
    env = {key: value for key, value in os.environ.items() if not key.startswith("RF_REPLAY_")}
    env.update(RF_REPLAY_LEVEL="L1S2.rfl", RF_REPLAY_ARCHIVE="levels1.vpp",
               RF_REPLAY_ACTOR_UID="8122")
    for phase, replay, extra in (
        ("save", inputs, {"RF_REPLAY_QUICKSAVE_FRAME": "270"}),
        ("load", neutral, {"RF_REPLAY_WORLD_SNAPSHOT_IN": str(run / "redfaction-save")})):
        process = subprocess.run([str(ROOT / "build/pc/Release/rf_pc_play.exe"),
                                  "--spawn-telemetry-replay", str(ROOT / "Installed_Game"), str(replay)],
                                 cwd=run, env=dict(env, **extra), capture_output=True, text=True)
        log = process.stdout + process.stderr
        (run / f"{phase}.log").write_text(log)
        process.check_returncode()
        if phase == "save":
            if ("DRILL_CUT 249 0 0 1 " not in log or "QUICK_SAVE frame270 status0" not in log or
                    "WORLD_SNAPSHOT_COMPONENT destruction " not in log):
                raise AssertionError("Post-cut ordinary quick-save did not complete")
        else:
            if ("WORLD_SNAPSHOT_LOADED " not in log or "VEHICLE_SAVE_RESTORE profile1 " not in log or
                    not re.search(r"^TERRAIN_PUBLICATION 171 707 1 1 ", log, re.M)):
                raise AssertionError("Fresh load did not restore the cut and seated Driller")
    return {"result": "PASS", "scope": "Text-only live L1S2 cut, ordinary quick-save and fresh PC load"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/driller-campaign-contact")
    parser.add_argument("--save-load", action="store_true", help="Also check ordinary post-cut save and fresh load without images")
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
    if (not contacts or contacts[0]["room"] != 8 or contacts[0]["face"] != 768
            or contacts[0]["owner_uid"] != 8123 or contacts[0]["status"] != 0
            or contacts[0]["accepted"] != 1):
        raise AssertionError("authored Driller did not commit the expected first wall cut")
    if not re.search(r"^AUTHORED_SOURCE_CUTS 8123 1(?: 0){6}$", log, re.M):
        raise AssertionError("L1S2 source did not retain one committed cut")
    publication = re.search(r"^TERRAIN_PUBLICATION (\d+) (\d+) (\d+) (\d+) ", log, re.M)
    if not publication or int(publication.group(1)) < 83 or publication.group(3, 4) != ("1", "1"):
        raise AssertionError("committed cut did not publish the room geometry")
    print(json.dumps(dict(result="PASS", level="L1S2.rfl", actor_uid=8122,
                          contacts=contacts, published_faces=int(publication.group(1)),
                          scope="First live PC cut and publication; collision hole and later cuts remain open")))
    if args.save_load:
        print(json.dumps(check_world_save(output, inputs)))


if __name__ == "__main__":
    main()
