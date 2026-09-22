"""Report real ordinary-level checkpoint capture blockers without altering assets.
This is a diagnostic replay, not save/load acceptance. No host input or UI control.
"""
import json, os, re, struct, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]

def main():
    folder = ROOT / 'artifacts/ordinary-save-readiness'
    folder.mkdir(parents=True, exist_ok=True)
    for name in ('report.json', 'frame.ppm'):
        (folder / name).unlink(missing_ok=True)
    frame = struct.pack('<5f7I', *([0] * 12))
    (folder / 'input.bin').write_bytes(b'RFI6' + struct.pack('<I', 48) + frame * 120)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RF_REPLAY_', 'RF_DEV_'))}
    env.update(RF_REPLAY_LEVEL='L1S1.rfl', RF_REPLAY_ARCHIVE='levels1.vpp', RF_REPLAY_WORLD_CHECKPOINT_PROBE='1')
    with (folder / 'run.log').open('wb') as log:
        run = subprocess.run([str(ROOT / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
            str(ROOT / 'Installed_Game'), str(folder / 'input.bin'), str(folder / 'frame.ppm')],
            cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    text = (folder / 'run.log').read_text(errors='replace')
    components = {name: dict(status=int(status), bytes=int(size)) for name, status, size in
        re.findall(r'WORLD_CHECKPOINT_PROBE (\w+) status(-?\d+) bytes(\d+)', text)}
    details = [line for line in text.splitlines() if line.startswith(('WORLD_CHECKPOINT_NPC ', 'WORLD_CHECKPOINT_NPC_ADMISSION ', 'WORLD_CHECKPOINT_NPC_SCRIPT ', 'WORLD_CHECKPOINT_EVENT ', 'WORLD_CHECKPOINT_PLAYER '))]
    reached = 'WORLD_CHECKPOINT_PROBE_ONLY no_save_written' in text
    result = dict(status='STARTUP_OR_REPLAY_FAILED' if run.returncode or not reached else
        'CAPTURE_BLOCKED' if details or any(v['status'] for v in components.values()) else 'COMPONENT_CAPTURE_READY_RESTORE_UNIMPLEMENTED',
        exit_code=run.returncode, level='Installed_Game/levels1.vpp:L1S1.rfl', frames=120,
        component_capture=components, details=details, save_written=False, reload_verified=False,
        visual_content_verified=False, native_memory_verified=False)
    (folder / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
