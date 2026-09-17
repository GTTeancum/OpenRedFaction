"""Selected ctf06 posts: live cuts, exact save continuation and cross-source rejection."""
import json, os, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
folder = ROOT / "artifacts/selected-post-live"
folder.mkdir(parents=True, exist_ok=True)
source = (ROOT / "artifacts/geomod-postedit-re/detached-live-verified/control.bin").read_bytes()
idle = source[:8] + bytes(201 * 48)
env = {k: v for k, v in os.environ.items() if not k.startswith(("RF_REPLAY_", "RF_DEV_"))}
env.update(RF_REPLAY_LEVEL="ctf06.rfl", RF_REPLAY_ARCHIVE="levelsm.vpp",
           RF_REPLAY_DEV_ROOM="1", RF_REPLAY_PLAYER_CHECKPOINT="1")
def run(uid, name, data, checkpoint=None, reject=False):
    path = folder / name
    path.with_suffix(".bin").write_bytes(data)
    path.with_suffix(".rfcp").unlink(missing_ok=True)
    local = dict(env, RF_REPLAY_AUTHORED_SOURCE=str(uid),
                 RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(path.with_suffix(".rfcp")))
    if checkpoint:
        local["RF_REPLAY_GEOMOD_CHECKPOINT_IN"] = str(checkpoint)
    with path.with_suffix(".log").open("wb") as log:
        result = subprocess.run([str(ROOT / "build/pc/Release/rf_pc_play.exe"), "--spawn-replay",
            str(ROOT / "Installed_Game"), str(path.with_suffix(".bin")), str(path.with_suffix(".ppm"))],
            cwd=ROOT, env=local, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    lines = path.with_suffix(".log").read_text().splitlines()
    if reject:
        assert result.returncode != 0 and not path.with_suffix(".rfcp").exists(), name
        assert "GEOMOD_CHECKPOINT_ERROR load -2" in lines, lines[-20:]
        return {"rejected": True}
    assert result.returncode == 0, name
    def values(label):
        return list(map(int, next(x for x in lines if x.startswith(label + " ")).split()[1:]))
    assert values("AUTHORED_SOURCE")[0] == uid
    pieces = values("DETACHED_PIECES")
    assert pieces[2] == 1 and pieces[5] == 0, (name, pieces)
    return {"pieces": pieces, "rockets": values("ROCKETS")}
report = {}
for uid in (93, 94, 96, 97):
    base = run(uid, str(uid), source)
    assert base["rockets"][0] == 1 and base["rockets"][4] == 1
    run(uid, f"{uid}-continued", idle, folder / f"{uid}.rfcp")
    run(uid, f"{uid}-control", source[:-48] + idle[8:])
    assert (folder / f"{uid}-continued.rfcp").read_bytes() == (folder / f"{uid}-control.rfcp").read_bytes(), uid
    report[str(uid)] = dict(base, exact_continuation=True)
    print(uid, "PASS", flush=True)
report["wrong_source"] = run(97, "wrong-source", idle, folder / "93.rfcp", reject=True)
report["scope"] = "One selected source per session; simultaneous sources and native runtime remain unverified."
(folder / "report.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
