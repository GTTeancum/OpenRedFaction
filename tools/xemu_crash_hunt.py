"""Bounded Xbox crash probes using isolated disc flags and read-only QMP telemetry.

No host input, capture, save/load scenarios, original HDD or tester-disc edits.
Build crash-disc/default.xbe separately before running this script.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time

from xemu_guest_snapshot import words, snapshot
from xemu_host import SessionLock, stop_owned_process
from xemu_session_guard import require_no_project_xemu
from xemu_smoke import Monitor
from xemu_world_hdd import standalone

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--level', default='L1S1.rfl')
    parser.add_argument('--archive', default='levels1.vpp')
    parser.add_argument('--actor', type=int)
    parser.add_argument('--dev-room', action='store_true')
    parser.add_argument('--firearms', type=int, choices=(1,2,3,4))
    parser.add_argument('--input', choices=('neutral','exercise'), default='neutral')
    parser.add_argument('--frames', type=int, default=1800)
    parser.add_argument('--seconds', type=int, default=180)
    args = parser.parse_args()
    if not 1 <= args.frames <= 60000 or not 1 <= args.seconds <= 180:
        parser.error('Require 1..60000 frames and 1..180 seconds')
    if args.dev_room and (args.level != 'ctf06.rfl' or args.archive != 'levelsm.vpp'):
        parser.error('This crash-hunt developer profile uses ctf06.rfl from levelsm.vpp')
    if args.firearms and not args.dev_room:
        parser.error('--firearms requires --dev-room')
    if any(len(s.encode('ascii')) >= 64 or '/' in s or '\\' in s for s in (args.level, args.archive)):
        parser.error('Require simple archive and level names')
    lock = SessionLock(ROOT)
    lock.acquire()
    process = monitor = None
    run = ROOT / 'artifacts/crash-hunt' / (datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + args.level)
    result = {'recipe': vars(args), 'input_recipe_version': 2, 'status': 'NOT_RUN'}
    eyes = set(); max_shots = max_switches = 0
    try:
        require_no_project_xemu(ROOT)
        disc = ROOT / 'build/xbox/crash-disc'
        base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
        standalone(base)
        # Cleanup may prune regenerable archives, so restore only this harness's
        # missing immutable inputs through hardlinks, never overwrite assets.
        for source in (ROOT / 'build/xbox/tester-disc').iterdir():
            if source.name in ('default.xbe','tester-profile.flag'): continue
            dest = disc / source.name
            if source.is_file() and not dest.exists(): dest.hardlink_to(source)
        run.mkdir(parents=True)
        mapping = (ROOT / 'build/xbox/main.map').read_text()
        shutil.copyfile(ROOT / 'build/xbox/main.map', run / 'main.map')
        result['xbe_sha256'] = hashlib.sha256((disc / 'default.xbe').read_bytes()).hexdigest()
        result['map_sha256'] = hashlib.sha256((ROOT / 'build/xbox/main.map').read_bytes()).hexdigest()
        # This folder is exclusively owned by this harness; no other staging is touched.
        for name in ('campaign-actor.bin', 'campaign-level.bin', 'campaign-spawn.flag', 'player-replay.bin', 'dev-room.flag', 'firearms-test.flag'):
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        (disc / 'player-control.flag').write_bytes(b'')
        (disc / 'scene-preview.flag').write_bytes(b'')
        (disc / 'diagnostic-checksums-off.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(args.archive.encode().ljust(64,b'\0')+args.level.encode().ljust(64,b'\0'))
        rows = []
        for frame in range(args.frames):
            move = [0., 0., 0.]; look = [0., 0.]; buttons = [0]*7
            if args.input == 'exercise' and frame >= 60:
                phase = (frame-60) % 600
                # One-second releases separate edges. All input stays inside
                # player_poll_paced; no OS/device injection is involved.
                if phase < 120: move[2] = .65
                elif phase < 240: move[0] = .5; look[0] = .2
                elif phase < 360: buttons[3] = 1  # fire
                elif phase < 420: buttons[6] = 1  # alternate fire
                elif phase == 420: buttons[5] = 1  # next weapon, before reload lock
                elif phase == 450: buttons[4] = 1  # reload
                elif phase == 480: buttons[2] = 1  # Use
                elif phase == 510: buttons[1] = 1  # jump
                elif phase >= 540: buttons[0] = 1; move[2] = -.4
            rows.append(struct.pack('<5f7I',*move,*look,*buttons))
        replay = b'RFI6'+struct.pack('<I',48)+b''.join(rows)
        (disc / 'player-replay.bin').write_bytes(replay)
        result['replay_sha256'] = hashlib.sha256(replay).hexdigest()
        if args.dev_room: (disc / 'dev-room.flag').write_bytes(b'1')
        if args.firearms: (disc / 'firearms-test.flag').write_text(str(args.firearms))
        if args.actor is not None:
            (disc / 'campaign-actor.bin').write_bytes(struct.pack('<I',args.actor))
        iso = ROOT / 'build/xbox/redfaction-crash.iso'
        iso.unlink(missing_ok=True)
        with (run / 'pack.log').open('wb') as log:
            subprocess.run(['C:/nxdk/tools/extract-xiso/build/extract-xiso.exe','-c',str(disc),str(iso)],check=True,stdout=log,stderr=subprocess.STDOUT)
        emu = Path('C:/Games/Emulators/Xemu')
        shutil.copyfile(emu/'EEPROM/eeprom.bin',run/'eeprom.bin')
        config = f'''[general]
show_welcome = false
skip_boot_anim = true
[general.updates]
check = false
[input]
auto_bind = false
background_input_capture = false
[net]
enable = false
[audio]
use_dsp = true
[sys.files]
bootrom_path = '{emu.as_posix()}/MCPX/mcpx_1.0.bin'
flashrom_path = '{emu.as_posix()}/BIOS/xbox-4627_debug.bin'
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{base.as_posix()}'
dvd_path = '{iso.as_posix()}'
'''
        (run/'xemu.toml').write_text(config)
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
        command=[str(emu/'xemu.exe'),'-config_path',str(run/'xemu.toml'),'-m','64','-snapshot','-display','xemu','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        counts={'rf_diagnostic':58,'rf_scene_enemy_failure':6,'rf_scene_tactical_failure':3,'rf_scene_enemy_combat':8,'rf_scene_profile_stage':2,'rf_xbox_load_stage':1,'rf_scene_campaign_load_stage':1,'rf_scene_campaign_load_detail':1,'rf_scene_npc_body_admission':4,'rf_player_replay_diagnostic':4,'rf_scene_actor_frame_count':1,'rf_scene_player_life':8,'rf_scene_gameplay_eye':3,'rf_scene_combat':8,'rf_scene_weapon_selection':8,'rf_preview_failure':8,'rf_scene_world_texture_budget':4}
        symbols={}
        for name,count in counts.items():
            match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',mapping)
            if match:symbols[name]=(int(match[1],16),count)
        require_no_project_xemu(ROOT)
        (run/'temp').mkdir()
        with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
            process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_AUDIO_DRIVER='dummy',TEMP=str(run/'temp'),TMP=str(run/'temp'),TMPDIR=str(run/'temp')))
        result['pid']=process.pid
        deadline=time.monotonic()+args.seconds
        while time.monotonic()<deadline and process.poll() is None:
            try:
                monitor=Monitor(port);monitor.sock.settimeout(3);break
            except OSError:time.sleep(.2)
        if monitor is None:raise RuntimeError('No QMP connection')
        memory=monitor.command('query-memory-size-summary');result['memory']=memory
        if memory['base-memory']!=64*1024*1024 or memory.get('plugged-memory',0):raise RuntimeError('Not stock64')
        result['status']='TIMEOUT'
        with (run/'telemetry.jsonl').open('w',buffering=1) as log:
            while time.monotonic()<deadline and process.poll() is None:
                try:sample={name:words(monitor,addr,count) for name,(addr,count) in symbols.items()}
                except RuntimeError as exc:
                    result['last_read_error']=str(exc);time.sleep(.2);continue
                log.write(json.dumps(sample)+'\n');result['last']=sample
                if sample['rf_player_replay_diagnostic'][2]:
                    eyes.add(tuple(sample['rf_scene_gameplay_eye']))
                    max_shots=max(max_shots,sample['rf_scene_combat'][0])
                    max_switches=max(max_switches,sample['rf_scene_weapon_selection'][1])
                result['observed']={'distinct_eye_positions':len(eyes),'shots':max_shots,'weapon_switches':max_switches}
                diagnostic=sample['rf_diagnostic']
                if diagnostic[2]&0x80000000:
                    result['status']='GUEST_ERROR'
                    (run/'failure-full.json').write_text(json.dumps(snapshot(monitor,mapping),indent=2))
                    break
                if diagnostic[1]==9 and diagnostic[37]>=args.frames and sample['rf_player_replay_diagnostic'][2]>=args.frames:
                    result['status']='COMPLETED';break
                time.sleep(.3)
        print(json.dumps({'run':str(run),'status':result['status'],'observed':result.get('observed'),'enemy_failure':result.get('last',{}).get('rf_scene_enemy_failure')}),flush=True)
    except Exception as exc:
        result['exception']=str(exc)
        result['status']='HARNESS_ERROR'
        raise
    finally:
        if monitor:
            try:monitor.command('quit')
            except Exception:pass
            try:monitor.close()
            except Exception:pass
        if process:
            stop_owned_process(process)
            result['exit_code']=process.poll()
        if run.exists():(run/'result.json').write_text(json.dumps(result,indent=2))
        lock.close()


if __name__=='__main__':main()
