"""Bounded stock64MiB render/campaign run: native framebuffer/profiles and PC gameplay checks.

Only process-local guest replay and QMP; no host input or desktop capture.
This is a renderer smoke check, not the complete campaign/parity suite.
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
import sys
import time

from PIL import Image
from xemu_guest_snapshot import words
from xemu_smoke import Monitor
from xemu_session_guard import require_no_project_xemu


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpu-exceptions', action='store_true', help='Retain QEMU exception/reset diagnostics for guest crash analysis')
    parser.add_argument('--dev-room', action='store_true', help='Supply supported weapons in Glass House only')
    parser.add_argument('--terrain-test-light', action='store_true', help='DEV crater diagnostic light during frames1000..1999')
    parser.add_argument('--frames', type=int, default=180)
    parser.add_argument('--seconds', type=int, default=180, help='Guest wall-clock deadline,30..3600 seconds (default180)')
    parser.add_argument('--level', default='L1S1.rfl')
    parser.add_argument('--archive', choices=['levels1.vpp','levels2.vpp','levels3.vpp','levelsm.vpp'], default='levels1.vpp')
    parser.add_argument('--spawn', action='store_true', help='Use authored player spawn without actor/item staging')
    parser.add_argument('--goal-uid', type=int, help='Authored goal setter at frame30')
    parser.add_argument('--actor', type=int, default=9858)
    parser.add_argument('--item-uid', type=int, help='Stage near an authored L1S1 pickup instead of an actor')
    parser.add_argument('--input', type=Path, help='Optional process-local replay; its length supplies the frame count')
    parser.add_argument('--setup-uid', type=int, nargs='+', default=[], help='Authored setup event at frame0, optionally another at frame60')
    parser.add_argument('--exit-start-uid', type=int, help='Stage once outside a real exit volume; replay must walk into it')
    parser.add_argument('--trigger-start-uid', type=int, help='Place inside an authored trigger; normal eligibility and Use still apply')
    parser.add_argument('--exit-uid', type=int, help='Authored exit at frame60')
    parser.add_argument('--return-exit-uid', type=int, help='Authored return at frame180, restaging the initial pickup')
    parser.add_argument('--visible', action='store_true')
    visibility = parser.add_mutually_exclusive_group()
    visibility.add_argument('--culled', dest='culled', action='store_true', help='Experimental model bounds rejection; default off after negative timing result')
    visibility.add_argument('--unculled', dest='culled', action='store_false')
    parser.set_defaults(culled=False)
    parser.add_argument('--unbatched', action='store_true', help='Reference tiny GPU command submission blocks')
    parser.add_argument('--unsorted', action='store_true', help='Reference source-order world draw ranges')
    args = parser.parse_args()
    if args.dev_room and (args.level != 'glass_house.rfl' or args.archive != 'levelsm.vpp' or not args.spawn):
        parser.error('Developer room requires --spawn --level glass_house.rfl --archive levelsm.vpp')
    if args.terrain_test_light and not args.dev_room:
        parser.error('--terrain-test-light requires --dev-room')
    payload = None
    if args.input:
        payload = args.input.read_bytes()
        size = {b'RFI2':28, b'RFI3':32, b'RFI4':40, b'RFI5':44, b'RFI6':48}.get(payload[:4],24)
        offset = 8 if size != 24 else 0
        if (offset and payload[4:8] != struct.pack('<I',size)) or len(payload)<=offset or (len(payload)-offset)%size:
            parser.error('Malformed replay input')
        args.frames = (len(payload)-offset)//size
    if len(args.setup_uid)>2 or any(not 0<uid<0xffffffff for uid in args.setup_uid):
        parser.error('Require at most two positive setup UIDs')
    if not 32 <= args.frames <= 60000 or not 30 <= args.seconds <= 3600 or not 0 < args.actor < 0xffffffff:
        parser.error('Require32..60000 frames,30..3600 seconds and a positive actor UID')
    if payload is None:
        payload = b'RFI5' + struct.pack('<I', 44) + bytes(args.frames * 44)
    if args.item_uid is not None and not 0 < args.item_uid < 0xffffffff:
        parser.error('Require a positive item UID')
    if any(v is not None and not 0<v<0xffffffff for v in (args.exit_uid,args.return_exit_uid)) or (args.return_exit_uid and not args.exit_uid):
        parser.error('Require positive exit UIDs and an outbound exit for a return')
    if not re.fullmatch(r'[A-Za-z0-9_-]+\.rfl',args.level) or len(args.level)>63 or (args.goal_uid is not None and not 0<args.goal_uid<0xffffffff) or (args.spawn and args.item_uid):
        parser.error('Require a plain level filename, positive goal UID and one placement mode')
    if args.exit_start_uid is not None and (not args.spawn or not 0<args.exit_start_uid<0xffffffff or args.exit_uid or args.return_exit_uid):
        parser.error('Exit-start requires --spawn, a positive UID and no forced exit options')
    root = Path(__file__).resolve().parents[1]
    if args.trigger_start_uid is not None and (not args.spawn or not 0<args.trigger_start_uid<0xffffffff or args.exit_start_uid):
        parser.error('Trigger-start requires --spawn, a positive UID and no exit-start placement')
    require_no_project_xemu(root)
    emulator = Path('C:/Games/Emulators/Xemu')
    disc = root / 'build/xbox/disc'
    run = root / 'artifacts/xemu' / ('render-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    print('Run:', run, flush=True)
    report = dict(result='FAIL', frames=args.frames, actor=None if args.item_uid or args.spawn else args.actor, level=args.level, archive=args.archive, goal_uid=args.goal_uid, item_uid=args.item_uid, model_culling=args.culled, command_batching=not args.unbatched, world_grouping=not args.unsorted,
        input_sha256=hashlib.sha256(payload).hexdigest(), setup_uids=args.setup_uid, trigger_start_uid=args.trigger_start_uid, exit_start_uid=args.exit_start_uid, exit_uid=args.exit_uid, return_exit_uid=args.return_exit_uid,
        scope='Authored section, player spawn or staged actor/pickup camera, process-local replay/setup commands, native framebuffer, '
              'phase timings and selected PC gameplay-state checks. No full campaign/parity claim.', samples=[])
    (run / 'inputs.bin').write_bytes(payload)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_LEVEL=args.level, RF_REPLAY_ARCHIVE=args.archive)
    if args.dev_room:env['RF_REPLAY_DEV_ROOM']='1'
    if args.terrain_test_light:env['RF_REPLAY_TERRAIN_TEST_LIGHT']='1'
    report['terrain_test_light']=args.terrain_test_light
    if args.terrain_test_light:
        report['terrain_test_light_scope']='Synthetic point source active at frames1000..1999; inspect framebuffer for visible lighting. No authored light or destruction fidelity claim.'
    if not args.spawn:env['RF_REPLAY_ACTOR_UID']=str(args.actor)
    if args.goal_uid:env['RF_REPLAY_GOAL_UID']=str(args.goal_uid)
    if args.item_uid:
        env.pop('RF_REPLAY_ACTOR_UID')
        env['RF_REPLAY_ITEM_UID']=str(args.item_uid)
    if args.exit_start_uid:env['RF_REPLAY_EXIT_START']=str(args.exit_start_uid)
    if args.trigger_start_uid:env['RF_REPLAY_TRIGGER_UID']=str(args.trigger_start_uid)
    if args.exit_uid:env['RF_REPLAY_EXIT_UID']=str(args.exit_uid)
    if args.return_exit_uid:
        env['RF_REPLAY_RETURN_EXIT_UID']=str(args.return_exit_uid)
        if args.item_uid:env['RF_REPLAY_RETURN_ITEM_UID']=str(args.item_uid)
    if args.setup_uid:
        env['RF_REPLAY_SETUP_UID']=','.join(map(str,args.setup_uid))
    # Build the reference before capture; a new Xbox build must not be compared
    # against a stale PC executable after shared source/allocation changes.
    with (run / 'pc-build.log').open('w') as out:
        subprocess.run(['cmake','--build','build/pc','--config','Release','--target','rf_pc_play'],
            cwd=root,stdout=out,stderr=subprocess.STDOUT,check=True)
    pc = subprocess.run([str(root / 'build/pc/Release/rf_pc_play.exe'), '--spawn-replay',
        str(root / 'Installed_Game'), str(run / 'inputs.bin'), str(run / 'pc-final.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    (run / 'pc-reference.txt').write_text(pc.stdout + pc.stderr)
    pc.check_returncode()
    report['pc_sha256'] = hashlib.sha256((root / 'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest()
    saved = {p.name: p.read_bytes() for p in disc.glob('campaign-*') if p.is_file()}
    for name in ('player-replay.bin', 'player-control-frames.txt', 'audio-output.flag', 'particle-step-fixtures.bin', 'renderer-cull-off.flag', 'renderer-cull-on.flag', 'renderer-batch-off.flag', 'renderer-world-off.flag'):
        p = disc / name
        saved[name] = p.read_bytes() if p.exists() else None
    for name in ('campaign-spawn.flag', 'campaign-level.bin', 'campaign-actor.bin', 'campaign-setup.bin', 'campaign-item.bin', 'campaign-exit.bin', 'campaign-return.bin', 'campaign-goal.bin', 'campaign-exit-start.bin'):
        saved.setdefault(name, None)
    process = monitor = None
    saved.setdefault('campaign-trigger-start.bin', None)
    dev_flag=disc/'dev-room.flag'
    saved['dev-room.flag']=dev_flag.read_bytes() if dev_flag.exists() else None
    light_flag=disc/'terrain-test-light.flag'
    saved[light_flag.name]=light_flag.read_bytes() if light_flag.exists() else None
    # Persist restoration bytes before mutating the disc, including absent files.
    (run / 'disc-restore.json').write_text(json.dumps({
        name: data.hex() if data is not None else None for name, data in saved.items()}))
    mapping = ''

    def build():
        with (run / 'build.log').open('ab') as out:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                'tools/build-xbox.sh', '--repack'], cwd=root, env=dict(os.environ, MSYSTEM='CLANG64'),
                stdout=out, stderr=subprocess.STDOUT, check=True)

    def symbol(name):
        match = re.search('_' + name + r'\s+([0-9a-fA-F]+)', mapping)
        if not match:
            raise ValueError('Missing symbol ' + name)
        return int(match[1], 16)

    def capture(d):
        if d[32] and d[33:35] == [640, 480] and d[35] >= 2560:
            path = run / 'framebuffer.bin'
            monitor.command('human-monitor-command', {'command-line':
                f'pmemsave 0x{d[32] & 0x03ffffff:x} {d[35] * d[34]} "{path.as_posix()}"'})
            Image.frombytes('RGB', (d[33], d[34]), path.read_bytes(), 'raw', 'BGRX', d[35], 1).save(run / 'framebuffer.png')

    try:
        for name in saved:
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        if args.dev_room:(disc/'dev-room.flag').write_bytes(b'')
        if args.terrain_test_light:(disc/'terrain-test-light.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(args.archive.encode().ljust(64, b'\0') + args.level.encode().ljust(64, b'\0'))
        if args.goal_uid:(disc/'campaign-goal.bin').write_bytes(struct.pack('<I',args.goal_uid))
        if not args.spawn:(disc / ('campaign-item.bin' if args.item_uid else 'campaign-actor.bin')).write_bytes(struct.pack('<I', args.item_uid or args.actor))
        if args.exit_start_uid:(disc/'campaign-exit-start.bin').write_bytes(struct.pack('<I',args.exit_start_uid))
        if args.trigger_start_uid:(disc/'campaign-trigger-start.bin').write_bytes(struct.pack('<I',args.trigger_start_uid))
        if args.exit_uid:(disc/'campaign-exit.bin').write_bytes(struct.pack('<I',args.exit_uid))
        if args.return_exit_uid:(disc/'campaign-return.bin').write_bytes(struct.pack('<II',args.return_exit_uid,args.item_uid or 0))
        if args.setup_uid:
            (disc / 'campaign-setup.bin').write_bytes(struct.pack('<'+'I'*len(args.setup_uid),*args.setup_uid))
        (disc / 'player-replay.bin').write_bytes(payload)
        if args.culled:
            (disc / 'renderer-cull-on.flag').write_bytes(b'')
        if args.unbatched:
            (disc / 'renderer-batch-off.flag').write_bytes(b'')
        if args.unsorted:
            (disc / 'renderer-world-off.flag').write_bytes(b'')
        build()
        mapping = (root / 'build/xbox/main.map').read_text()
        (run / 'main.map').write_text(mapping)
        shutil.copyfile(disc / 'default.xbe', run / 'default.xbe')
        report['xbe_sha256'] = hashlib.sha256((run / 'default.xbe').read_bytes()).hexdigest()
        hdd = root / 'local/xemu-harness/pacing-base.qcow2'
        if not hdd.exists():
            raise ValueError('Missing isolated pacing HDD base')
        shutil.copyfile(emulator / 'eeprom.bin', run / 'eeprom.bin')
        config = run / 'xemu.toml'
        config.write_text(f'''[general]
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
bootrom_path = '{emulator.as_posix()}/MCPX/mcpx_1.0.bin'
flashrom_path = '{emulator.as_posix()}/BIOS/xbox-4627_debug.bin'
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{root.as_posix()}/build/xbox/redfaction-diagnostic.iso'
''')
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        command = [str(emulator / 'xemu.exe'), '-config_path', str(config), '-m', '64', '-snapshot',
            '-display', 'xemu', '-audio', 'none', '-qmp', f'tcp:127.0.0.1:{port},server=on,wait=off']
        if args.cpu_exceptions:command += ['-d','int,cpu_reset','-D',str(run/'cpu-exceptions.log')]
        report['command'] = command
        startup = None
        if os.name == 'nt' and not args.visible:
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (run / 'stdout.log').open('wb') as out, (run / 'stderr.log').open('wb') as err:
            require_no_project_xemu(root)
            process = subprocess.Popen(command, cwd=run, env=dict(os.environ, SDL_AUDIO_DRIVER='dummy'),
                stdout=out, stderr=err, startupinfo=startup,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            report.update(pid=process.pid, port=port)
            (run / 'live.json').write_text(json.dumps(dict(pid=process.pid, port=port)))
            deadline = time.monotonic() + args.seconds
            previous = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'XEMU exited {process.returncode}')
                if monitor is None:
                    try:
                        monitor = Monitor(port)
                    except OSError:
                        time.sleep(.5)
                        continue
                    report['memory'] = monitor.command('query-memory-size-summary')
                    assert report['memory']['base-memory'] == 64 * 1024 * 1024
                try:
                    d = words(monitor, symbol('rf_diagnostic'), 58)
                except RuntimeError as exc:
                    if 'received 0' not in str(exc):
                        raise
                    time.sleep(.5)
                    continue
                if d[0] != 0x52464447:
                    time.sleep(.5)
                    continue
                if (d[2]==1 or (d[2]==2 and d[37]==0)) and any(sample['phase']==2 and sample['frame']>0 for sample in report['samples']) and not words(monitor,symbol('rf_xbox_level_transitions'),1)[0]:
                    raise RuntimeError('Guest restarted after entering gameplay')
                report['samples'].append(dict(time=time.monotonic(), phase=d[2], frame=d[37], pages=d[44]))
                current = (d[2], d[37] // 30)
                if current != previous:
                    if d[2]==2 and d[37]>0:
                        budget_words=words(monitor,symbol('rf_scene_world_texture_budget'),4)
                        report.setdefault('world_texture_samples',[]).append(dict(frame=d[37],words=budget_words))
                        report.setdefault('airlock_samples',[]).append(dict(frame=d[37],words=words(monitor,symbol('rf_scene_airlock'),6)))
                    print('Guest phase', d[2], 'submitted', d[37], 'frames', flush=True)
                    previous = current
                if d[2] & 0x80000000:
                    raise RuntimeError(f'Guest error {d[2]:08x}')
                if d[2] == 5:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError('Bounded render run did not complete')
            monitor.command('stop')
            capture(d)
            assert d[37] == args.frames, (d[37], args.frames)
            fields = [('rf_diagnostic', 58), ('rf_xbox_retained_world', 8), ('rf_xbox_retained_models', 8),
                ('rf_xbox_retained_model_kinds', 6), ('rf_scene_pose_sharing', 4), ('rf_xbox_model_visibility', 8), ('rf_xbox_bounds_poses', 2), ('rf_xbox_command_blocks', 6), ('rf_xbox_world_groups', 2), ('rf_renderer_submission', 4), ('rf_renderer_vblank', 3)]
            fields += [(name, 32) for name in ('rf_renderer_profile', 'rf_scene_profile',
                'rf_scene_presentation_profile', 'rf_scene_world_profile', 'rf_scene_step_profile',
                'rf_scene_npc_step_profile', 'rf_scene_npc_playback_profile')]
            fields.append(('rf_scene_terrain_edit_times',40))
            snap = dict(symbols={name: dict(words=words(monitor, symbol(name), count)) for name, count in fields})
            (run / 'guest-memory-final.json').write_text(json.dumps(snap, indent=2))
            edits=snap['symbols']['rf_scene_terrain_edit_times']['words']
            (run/'terrain-edit-times.json').write_text(json.dumps([dict(zip(('frame','cut_ms','bind_ms','debris_prepare_ms','debris_spawn_ms'),edits[i:i+5])) for i in range(0,40,5) if edits[i]],indent=2)+'\n')
            report['checks'] = {}
            transition_rows=[line.split()[1:] for line in pc.stdout.splitlines() if line.startswith('LEVEL_TRANSITION ')]
            native_transition=words(monitor,symbol('rf_xbox_level_transitions'),4)
            target=struct.pack('<16I',*words(monitor,symbol('rf_xbox_transition_target'),16)).split(b'\0')[0].decode('ascii')
            expected_transition=[len(transition_rows),int(transition_rows[-1][2]),int(transition_rows[-1][3])] if transition_rows else [0,0,0]
            report['transitions']=dict(pc=transition_rows,xbox=native_transition,target=target)
            assert native_transition[:3]==expected_transition, 'Transition count/UID/frame mismatch'
            if transition_rows:assert target==transition_rows[-1][1], 'Destination mismatch'
            if args.exit_start_uid:
                assert len(transition_rows)==1 and int(transition_rows[0][2])==args.exit_start_uid, 'Walk did not reach its exit'

            goal_words=words(monitor,symbol('rf_scene_mission_goals'),4225)
            assert goal_words[0]<=64
            raw_goals=struct.pack('<4225I',*goal_words)
            native_goals=[]
            for i in range(goal_words[0]):
                at=4+i*264
                name=raw_goals[at:at+256].split(b'\0')[0].decode('cp1252')
                value,persistent=struct.unpack_from('<iI',raw_goals,at+256)
                native_goals.append(f'MISSION_GOAL {name} {value} {persistent}')
            pc_goals=[line for line in pc.stdout.splitlines() if line.startswith('MISSION_GOAL ')]
            report['mission_goals']=dict(xbox=native_goals,pc=pc_goals,equal=native_goals==pc_goals)
            assert native_goals==pc_goals,'Mission goal mismatch'
            for name, label, count in [('scene_actor_body', 'PC_PLAY_BODY', 77),
                    ('rf_scene_player_ammo', 'PLAYER_AMMO', 8), ('rf_scene_combat', 'COMBAT', 8),
                    ('rf_scene_script_movement', 'SCRIPT_MOVE', 8), ('rf_scene_enemy_combat', 'ENEMY_COMBAT', 8),
                    ('rf_scene_rotating_doors', 'ROTATING_DOORS', 8), ('rf_scene_script_attack', 'SCRIPT_ATTACK', 12), ('rf_scene_attack_recovery', 'ATTACK_RECOVERY', 4), ('rf_scene_enemy_damage_kinds', 'ENEMY_DAMAGE_KINDS', 10), ('rf_scene_enemy_melee', 'ENEMY_MELEE', 4), ('rf_scene_enemy_spread', 'ENEMY_SPREAD', 8), ('rf_scene_combat_pain', 'COMBAT_PAIN', 8), ('rf_scene_pain_attack_gate', 'PAIN_ATTACK_GATE', 6), ('rf_scene_weapon_drops', 'WEAPON_DROPS', 8), ('rf_scene_rifle_alt', 'RIFLE_ALT', 8), ('rf_scene_shotgun', 'SHOTGUN', 8), ('rf_scene_rockets', 'ROCKETS', 8), ('rf_scene_rocket_blast', 'ROCKET_BLAST', 8), ('rf_scene_rocket_visual', 'ROCKET_VISUAL', 8), ('rf_scene_enemy_fire', 'ENEMY_FIRE', 6),
                    ('rf_scene_use_reach', 'USE_REACH', 4),
                    ('rf_scene_particles_summary', 'SCENE_PARTICLES', 8), ('rf_scene_live_motion', 'LIVE_MOTION', 8), ('rf_scene_airlock', 'AIRLOCK', 6), ('rf_scene_script_animation', 'SCRIPT_ANIMATION', 10), ('rf_scene_alarm', 'ALARM', 12), ('rf_scene_switch_runtime', 'SWITCH_RUNTIME', 8), ('rf_scene_switch_detail', 'SWITCH_DETAIL', 8), ('rf_scene_switch_history', 'SWITCH_HISTORY', 4), ('rf_scene_trigger_history', 'TRIGGER_HISTORY', 4), ('rf_scene_startup_inventory', 'STARTUP_INVENTORY', 4), ('rf_scene_pickups', 'PICKUPS', 8), ('rf_scene_pickup_vitals', 'PICKUP_VITALS', 4), ('rf_scene_riot', 'RIOT_STICK', 8), ('rf_scene_weapon_selection', 'WEAPON_SELECTION', 8),
                    ('rf_scene_player_weapon', 'PLAYER_WEAPON', 8), ('rf_scene_weapon_audio', 'WEAPON_AUDIO', 9), ('rf_scene_impact_audio', 'IMPACT_AUDIO', 9),
                    ('rf_scene_combat_death', 'COMBAT_DEATH', 8)]:
                expected = list(map(int, next(line for line in pc.stdout.splitlines() if line.startswith(label + ' ')).split()[1:]))
                actual = words(monitor, symbol(name), count)
                # PICKUPS[6] counts CPU-emitted vertices. Xbox retained GPU
                # submission bypasses those vertices in scene_weapon_submit.
                indices=[i for i in range(count) if label!='PICKUPS' or i!=6]
                equal=all(actual[i]==expected[i] for i in indices)
                report['checks'][label] = dict(equal=equal, all_words_equal=actual==expected,
                    compared_indices=indices, xbox=actual, pc=expected)
                if label=='PICKUPS':
                    report['pickup_cpu_vertices']=dict(xbox=actual[6],pc=expected[6],
                        scope='Backend-specific rendering count; excluded from gameplay parity')
                assert equal, label
            if args.dev_room:
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('GEOMOD ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_geomod'),8)
                # Allocation sizes differ by pointer width. Compare terrain
                # presence/history/generation/status, and enforce both budgets.
                indices=[0,1,2,5,6,7]
                equal=all(actual[i]==expected[i] for i in indices)
                budget_ok=all(0<=v[3]<=v[4]<=1024*1024+65536 for v in (actual,expected))
                report['checks']['GEOMOD']=dict(equal=equal,budget_ok=budget_ok,
                    compared_indices=indices,xbox=actual,pc=expected,
                    scope='Terrain publication and bounded memory; geometry buffers require separate verification')
                assert equal and budget_ok,'GEOMOD'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_SHADOWS ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_shadows'),4)
                equal=actual[:3]==expected[:3]
                report['checks']['TERRAIN_SHADOWS']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Lighting rebuild/ray/occlusion counters; cache hits are backend draw-count dependent')
                assert equal,'TERRAIN_SHADOWS'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_ATLAS ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_atlas'),8)
                equal=actual==expected
                budget_ok=actual[3]<=1280*1024
                report['checks']['TERRAIN_ATLAS']=dict(equal=equal,budget_ok=budget_ok,xbox=actual,pc=expected,
                    scope='Atlas dimensions, bounded ownership, generation and sampled texels; pixels checked separately')
                assert equal and budget_ok,'TERRAIN_ATLAS'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_BAKE ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_bake'),6)
                equal=actual==expected
                report['checks']['TERRAIN_BAKE']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Deterministic lightmap fill progress; noise fills within atlas capacity, reference shadows retain64-texel bound')
                bound=512*512 if words(monitor,symbol('rf_scene_terrain_noise'),1)[0] else 64
                assert equal and actual[1]<=bound and actual[2]<=bound,'TERRAIN_BAKE'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_DRAW ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_draw'),5)
                equal=actual==expected
                report['checks']['TERRAIN_DRAW']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Render-only subdivision counts, bounded ownership and generation; physical mesh remains separate')
                assert equal and actual[1]<=8192 and actual[3]<=320*1024,'TERRAIN_DRAW'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_NOISE ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_noise'),8)
                equal=actual==expected
                report['checks']['TERRAIN_NOISE']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Persistent generated-face mappings, retained texel checks, bounded owner and generation')
                assert equal and actual[0]==1 and actual[1]<=1024 and actual[6]<=128*1024,'TERRAIN_NOISE'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DEBRIS ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_debris'),8)
                equal=actual==expected
                report['checks']['DEBRIS']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Bounded DEV pool: spawn/active/bounce/expiry, rendered vertices/hash, owned bytes and replacement count')
                assert equal and actual[1]<=80 and actual[6]<=65536,'DEBRIS'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_UPLOAD ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_upload'),4)
                equal=actual==expected
                report['checks']['TERRAIN_UPLOAD']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Dirty-rectangle update count and copied pixels; image content checked separately')
                assert equal,'TERRAIN_UPLOAD'



            report.update(result='PASS', available_pages=d[44], diagnostic=d)
            with (run / 'performance.txt').open('w') as out:
                subprocess.run([sys.executable, 'tools/summarize_xbox_performance.py',
                    str(run / 'guest-memory-final.json'), '--out', str(run / 'performance.json')],
                    cwd=root, stdout=out, check=True)
    except Exception as exc:
        report['error'] = repr(exc)
        if monitor:
            try:
                monitor.command('stop')
                capture(words(monitor, symbol('rf_diagnostic'), 58))
                report['registers'] = monitor.command('human-monitor-command', {'command-line': 'info registers'})
                report['failure_telemetry'] = {name: words(monitor, symbol(name), count) for name, count in
                    [('rf_xbox_renderer_stage',4), ('rf_diagnostic', 58), ('rf_animation_progress', 4), ('rf_scene_profile_stage', 2),
                     ('rf_xbox_retained_world', 8), ('rf_xbox_retained_models', 8), ('rf_xbox_retained_model_kinds', 6)]}
            except Exception as capture_error:
                report['capture_error'] = repr(capture_error)
        raise
    finally:
        if monitor:
            try:
                monitor.command('quit')
            except (OSError, RuntimeError):
                pass
            monitor.close()
        if process:
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=8)
        for name, data in saved.items():
            if data is None:
                (disc / name).unlink(missing_ok=True)
            else:
                (disc / name).write_bytes(data)
        try:
            build()
        except Exception as exc:
            report.update(result='FAIL', restore_error=repr(exc))
            raise
        finally:
            (run / 'report.json').write_text(json.dumps(report, indent=2))
            print(run, report['result'], flush=True)


if __name__ == '__main__':
    main()
