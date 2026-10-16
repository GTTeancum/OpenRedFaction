"""Bounded Xbox crash probes using isolated disc flags and game-written RAM logs.

The default is OpenJKDF2ogx's unchanged reader over HMP. Legacy QMP telemetry
requires explicit selection; neither transport supplies gameplay inputs.

No host input, capture, save/load scenarios, original HDD or tester-disc edits.
Build crash-disc/default.xbe separately before running this script.
"""
import argparse
import datetime
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import time

from xemu_guest_snapshot import words
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
    parser.add_argument('--distance', type=float, help='Stage this many units in front of --actor, preserving all authored entities')
    parser.add_argument('--spawn-y', type=float, help='Override only the staged player-start height; requires --distance')
    parser.add_argument('--loadout', type=int, choices=(1,2,4,5,6,8), help='Diagnostic pistol/rifle/shotgun/rocket/grenade/railgun start; ordinary finite ammunition')
    parser.add_argument('--aim', action='store_true', help='Process-local bounded look input toward live hostile actors')
    parser.add_argument('--aim-actor', type=int, help='Prioritize this authored UID through bounded look input; nearest-hostile fallback after loss')
    parser.add_argument('--aim-point', type=float, nargs=3, metavar=('X','Y','Z'),
                        help='Process-local bounded look input toward an explicit world point')
    parser.add_argument('--scenario', choices=('combat','explosives','mobile'), help='Game-owned live-tick phases; mobile adds sustained movement, explosives recovers the requested weapon through ordinary cycle inputs')
    parser.add_argument('--dev-room', action='store_true')
    parser.add_argument('--firearms', type=int, choices=(1,2,3,4))
    parser.add_argument('--input', choices=('neutral','exercise','combat','approach','approach-jump'), default='neutral')
    parser.add_argument('--frames', type=int, default=1800)
    parser.add_argument('--seconds', type=int, default=180)
    parser.add_argument('--log-protocol', choices=('qmp-telemetry','jkdf2-hmp'), default='jkdf2-hmp',
                        help='Default uses the unchanged OpenJKDF2 RAM log reader over HMP')
    parser.add_argument('--poll-seconds', type=float, default=1.0,
                        help='Application telemetry interval (0.3..5 seconds); larger intervals may miss short-lived states')
    args = parser.parse_args()
    if not 1 <= args.frames <= 60000 or not 1 <= args.seconds <= 600:
        parser.error('Require 1..60000 frames and 1..600 seconds')
    if not .3 <= args.poll_seconds <= 5:
        parser.error('Require 0.3..5 seconds between telemetry samples')
    if args.dev_room and (args.level != 'ctf06.rfl' or args.archive != 'levelsm.vpp'):
        parser.error('This crash-hunt developer profile uses ctf06.rfl from levelsm.vpp')
    if args.firearms and not args.dev_room:
        parser.error('--firearms requires --dev-room')
    if args.loadout and args.dev_room:
        parser.error('--loadout is for authored campaign encounters')
    if args.scenario and (args.input != 'neutral' or args.dev_room):
        parser.error('--scenario requires neutral replay and an authored encounter')
    if args.scenario=='explosives' and args.loadout not in (5,6):
        parser.error('--scenario explosives requires rocket or grenade loadout')
    if args.aim_actor is not None and (args.aim or args.aim_point or not 0 <= args.aim_actor < 2147483648):
        parser.error('--aim-actor requires a nonnegative UID and excludes other aim modes')
    if args.aim_point and (args.aim or any(not math.isfinite(x) or abs(x)>100000 for x in args.aim_point)):
        parser.error('--aim-point requires finite bounded coordinates and cannot be combined with --aim')
    if args.distance is not None and (args.actor is None or not 3 <= args.distance <= 20):
        parser.error('--distance requires --actor and a distance from 3 to 20')
    if args.spawn_y is not None and (args.distance is None or not math.isfinite(args.spawn_y) or abs(args.spawn_y)>100000):
        parser.error('--spawn-y requires --distance and a finite bounded height')
    if any(len(s.encode('ascii')) >= 64 or '/' in s or '\\' in s for s in (args.level, args.archive)):
        parser.error('Require simple archive and level names')
    lock = SessionLock(ROOT)
    lock.acquire()
    process = monitor = None
    ram_reader = None
    pending_samples = []
    seen_samples = set()
    ram_poll_index = 0
    run = ROOT / 'artifacts/crash-hunt' / (datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + args.level)
    result = {'recipe': vars(args), 'input_recipe_version': 6, 'status': 'NOT_RUN'}
    eyes = set(); max_shots = max_switches = 0
    max_enemy_shots = max_alerts = max_deaths = max_respawns = 0
    alive_samples = dead_samples = 0
    max_live_actors = max_alerted_actors = 0
    max_rockets = max_grenades = max_blast_kills = 0
    max_grenade_bounces = max_grenade_detonations = max_terrain_edits = max_debris = 0
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
        # Pin authored encounter membership; actor staging changes only the
        # player placement, never enemy count or original archive bytes.
        from build_fragment_platform_fixture import read_entry
        from xemu_turret_combat import entity_rows
        level_bytes = read_entry(ROOT/'Installed_Game'/args.archive,args.level)
        actors = entity_rows(level_bytes)
        if args.aim_actor is not None and not any(r['uid']==args.aim_actor for r in actors):
            raise ValueError('Priority aim UID is not authored in this level')
        census = {'level_sha256':hashlib.sha256(level_bytes).hexdigest(),
            'actors':[{'uid':r['uid'],'class':r['name'],
                'position':struct.unpack_from('<3f',r['raw'],r['transform'])} for r in actors]}
        (run/'authored-encounter.json').write_text(json.dumps(census,indent=2))
        result['authored_actor_count'] = len(actors)
        mapping = (ROOT / 'build/xbox/main.map').read_text()
        shutil.copyfile(ROOT / 'build/xbox/main.map', run / 'main.map')
        result['xbe_sha256'] = hashlib.sha256((disc / 'default.xbe').read_bytes()).hexdigest()
        result['map_sha256'] = hashlib.sha256((ROOT / 'build/xbox/main.map').read_bytes()).hexdigest()
        # This folder is exclusively owned by this harness; no other staging is touched.
        for name in ('campaign-actor.bin', 'campaign-level.bin', 'campaign-spawn.flag', 'player-replay.bin', 'dev-room.flag', 'firearms-test.flag', 'crash-loadout.bin', 'crash-aim.flag', 'crash-aim-point.bin', 'crash-aim-actor.bin', 'crash-scenario.flag', 'crash-ram-log.flag', 'crash-start.vpp', 'scene-fixture.vpp'):
            (disc / name).unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        (disc / 'player-control.flag').write_bytes(b'')
        (disc / 'scene-preview.flag').write_bytes(b'')
        (disc / 'diagnostic-checksums-off.flag').write_bytes(b'')
        (disc / 'campaign-level.bin').write_bytes(args.archive.encode().ljust(64,b'\0')+args.level.encode().ljust(64,b'\0'))
        if args.distance is not None:
            import io
            from inspect_levels import inspect
            from check_ai_projectile_ordinary import archive
            target = next(r for r in actors if r['uid'] == args.actor)
            pose = struct.unpack_from('<12f',target['raw'],target['transform'])
            position = [pose[i]+pose[3+i]*args.distance for i in range(3)]
            position[1] += .625
            if args.spawn_y is not None:position[1]=args.spawn_y
            forward = [-x for x in pose[3:6]]
            right = [forward[2],0.,-forward[0]]
            meta = inspect(io.BytesIO(level_bytes),dict(offset=0,size=len(level_bytes),name=args.level))
            start = next(s for s in meta['sections'] if s['type']=='0x70000')
            if start['size'] != 48: raise RuntimeError('Unexpected player-start layout')
            staged = bytearray(level_bytes)
            staged[start['offset']+8:start['offset']+56] = struct.pack('<12f',*position,*forward,*right,0.,1.,0.)
            archive(disc/'scene-fixture.vpp',[(args.level,staged)])
            (disc/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+args.level.encode().ljust(64,b'\0'))
            result['staged_start'] = {'position':position,'forward':forward,
                'level_sha256':hashlib.sha256(staged).hexdigest(),'changed_bytes':'player-start transform only'}
        rows = []
        for frame in range(args.frames):
            move = [0., 0., 0.]; look = [0., 0.]; buttons = [0]*7
            if args.input in ('approach','approach-jump'):
                # Focused contact probe: ordinary forward movement from the
                # first live frame, with no fire or host input.
                move[2] = .65
                if args.input == 'approach-jump' and frame == 12: buttons[1] = 1
            elif args.input == 'exercise' and frame >= 60:
                phase = (frame-60) % 600
                # One-second releases separate edges. All input stays inside
                # player_poll_paced; no OS/device injection is involved.
                if phase < 120: move[2] = .65
                elif phase < 240: move[0] = .5; look[1] = .2
                elif phase < 360: buttons[3] = 1  # fire
                elif phase < 420: buttons[6] = 1  # alternate fire
                elif phase == 420: buttons[5] = 1  # next weapon, before reload lock
                elif phase == 450: buttons[4] = 1  # reload
                elif phase == 480: buttons[2] = 1  # Use
                elif phase == 510: buttons[1] = 1  # jump
                elif phase >= 540: buttons[0] = 1; move[2] = -.4
            elif args.input == 'combat':
                phase = frame % 360
                move[0] = .35 if 60 <= phase < 150 else -.35 if 150 <= phase < 240 else 0
                # Release edges exercise semi-auto and burst weapons as well as
                # automatic ones; holding forever produces only one rifle burst.
                buttons[3] = int((phase < 180 or 240 <= phase < 330) and phase % 12 < 6)
                if phase == 180: buttons[4] = 1
                if phase == 330: buttons[5] = 1
                # Ordinary Use edge also requests the existing diagnostic death
                # restart. Do not alter damage, health or enemy state to survive.
                if phase == 220: buttons[2] = 1
                if phase == 300: buttons[1] = 1
            rows.append(struct.pack('<5f7I',*move,*look,*buttons))
        replay = b'RFI6'+struct.pack('<I',48)+b''.join(rows)
        (disc / 'player-replay.bin').write_bytes(replay)
        result['replay_sha256'] = hashlib.sha256(replay).hexdigest()
        if args.dev_room: (disc / 'dev-room.flag').write_bytes(b'1')
        if args.firearms: (disc / 'firearms-test.flag').write_text(str(args.firearms))
        if args.actor is not None and args.distance is None:
            (disc / 'campaign-actor.bin').write_bytes(struct.pack('<I',args.actor))
        if args.loadout:
            (disc / 'crash-loadout.bin').write_bytes(struct.pack('<I',args.loadout))
        if args.aim:
            (disc / 'crash-aim.flag').write_bytes(b'')
        if args.aim_actor is not None:
            (disc / 'crash-aim-actor.bin').write_bytes(struct.pack('<I',args.aim_actor))
        if args.aim_point:
            (disc / 'crash-aim-point.bin').write_bytes(struct.pack('<3f',*args.aim_point))
        if args.scenario:
            (disc / 'crash-scenario.flag').write_bytes(b'2' if args.scenario=='explosives' else b'3' if args.scenario=='mobile' else b'')
        if args.log_protocol=='jkdf2-hmp':
            (disc / 'crash-ram-log.flag').write_bytes(b'')
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
        if args.log_protocol=='jkdf2-hmp':
            command[-2:]=['-monitor',f'tcp:127.0.0.1:{port},server,nowait']
            reader_path=ROOT/'tools/vendor/openjkdf2/poll_xemu_ram_log.py'
            spec=importlib.util.spec_from_file_location('jkdf2_ram_log',reader_path)
            ram_reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(ram_reader)
            # LLD prints 32-bit VAs padded to sixteen digits; the unchanged
            # upstream reader expects eight. Strip only redundant zero padding.
            poll_map=run/'poll-build.map'
            poll_map.write_text(re.sub(r'\b00000000([0-9a-fA-F]{8})\b',r'\1',mapping))
            ram_symbols=ram_reader.resolve_symbols(str(poll_map),str(disc/'default.xbe'))
            ram_args=argparse.Namespace(timeout=3,phys_delta='auto',header_only=False)
            result['ram_reader_sha256']=hashlib.sha256(reader_path.read_bytes()).hexdigest()
            result['ram_poll_interval_seconds']=15
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
        counts={'rf_diagnostic':58,'rf_scene_enemy_failure':6,'rf_scene_tactical_failure':3,'rf_scene_enemy_combat':8,'rf_scene_combat_population':4,'rf_scene_enemy_awareness':8,'rf_scene_npc_registration':6,'rf_scene_player_ammo':8,'rf_scene_rockets':8,'rf_scene_rocket_blast':8,'rf_scene_grenades':8,'rf_scene_geomod':8,'rf_scene_debris':8,'rf_scene_profile_stage':2,'rf_xbox_load_stage':1,'rf_scene_campaign_load_stage':1,'rf_scene_campaign_load_detail':1,'rf_scene_npc_body_admission':4,'rf_scene_npc_body_failure':2,'rf_scene_patrol_failure':3,'rf_player_replay_diagnostic':4,'rf_scene_actor_frame_count':1,'rf_scene_player_life':8,'rf_scene_crash_activity':4,'rf_scene_crash_aim_trace':4,'rf_scene_crash_enemy_state':12,'rf_scene_enemy_aim':4,'rf_scene_gameplay_eye':3,'rf_scene_combat':8,'rf_scene_weapon_selection':8,'rf_preview_failure':8,'rf_scene_world_texture_budget':4}
        symbols={}
        counts['rf_scene_crash_scenario_trace']=8
        counts['rf_scene_crash_aim_point']=3
        counts['rf_scene_crash_aim']=1
        counts['rf_scene_crash_terrain']=8
        for name,count in counts.items():
            match=re.search(r'_'+name+r'\s+([0-9a-fA-F]+)',mapping)
            if match:symbols[name]=(int(match[1],16),count)
        if args.scenario and 'rf_scene_crash_scenario_trace' not in symbols:
            raise RuntimeError('Scenario runner requires an updated Xbox binary/map')
        if args.aim_point and 'rf_scene_crash_aim_point' not in symbols:
            raise RuntimeError('Aim point requires an updated Xbox binary/map')
        require_no_project_xemu(ROOT)
        (run/'temp').mkdir()
        with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
            process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_AUDIO_DRIVER='dummy',TEMP=str(run/'temp'),TMP=str(run/'temp'),TMPDIR=str(run/'temp')))
        result['pid']=process.pid
        deadline=time.monotonic()+args.seconds
        while not ram_reader and time.monotonic()<deadline and process.poll() is None:
            try:
                monitor=Monitor(port);monitor.sock.settimeout(3);break
            except OSError:time.sleep(.2)
        if not ram_reader:
            if monitor is None:raise RuntimeError('No QMP connection')
            memory=monitor.command('query-memory-size-summary');result['memory']=memory
            if memory['base-memory']!=64*1024*1024 or memory.get('plugged-memory',0):raise RuntimeError('Not stock64')
        result['status']='TIMEOUT'
        if ram_reader:time.sleep(15)  # Reference smoke runner waits before its first poll.
        with (run/'telemetry.jsonl').open('w',buffering=1) as log:
            while time.monotonic()<deadline and process.poll() is None:
                try:
                    if ram_reader:
                        if not pending_samples:
                            delta,header,ram_text=ram_reader.poll_port(port,ram_symbols,ram_args)
                            ram_poll_index+=1
                            (run/f'ram_poll_{ram_poll_index:04d}.txt').write_text(ram_text)
                            result['ram_header']=header;result['ram_physical_delta']=delta
                            for line in ram_text.splitlines():
                                if not line.startswith('RFLOG '):continue
                                try:value=json.loads(line[6:])
                                except json.JSONDecodeError:continue
                                key=(value['rf_player_replay_diagnostic'][2],value['rf_diagnostic'][2])
                                if key not in seen_samples:
                                    seen_samples.add(key);pending_samples.append(value)
                        if not pending_samples:
                            time.sleep(15);continue
                        sample=pending_samples.pop(0)
                        pages=sample['rf_diagnostic'][3]
                        result['memory']={'source':'game MmQueryStatistics','base-memory':pages*4096}
                        if pages!=16384:raise ValueError('Game log did not confirm stock64')
                    else:sample={name:words(monitor,addr,count) for name,(addr,count) in symbols.items()}
                except (RuntimeError,OSError) as exc:
                    result['last_read_error']=str(exc);time.sleep(15 if ram_reader else .2);continue
                sample['elapsed_seconds']=round(args.seconds-(deadline-time.monotonic()),3)
                log.write(json.dumps(sample)+'\n');result['last']=sample
                if sample['rf_player_replay_diagnostic'][2]:
                    if sample['rf_scene_player_life'][2]: dead_samples += 1
                    else: alive_samples += 1
                    eyes.add(tuple(sample['rf_scene_gameplay_eye']))
                    max_shots=max(max_shots,sample['rf_scene_combat'][0])
                    max_switches=max(max_switches,sample['rf_scene_weapon_selection'][1])
                    max_enemy_shots=max(max_enemy_shots,sample['rf_scene_enemy_combat'][2])
                    max_alerts=max(max_alerts,sample['rf_scene_enemy_combat'][1])
                    max_deaths=max(max_deaths,sample['rf_scene_player_life'][0])
                    max_respawns=max(max_respawns,sample['rf_scene_player_life'][1])
                    population=sample.get('rf_scene_combat_population',[0,0,0,0])
                    max_live_actors=max(max_live_actors,population[1])
                    max_alerted_actors=max(max_alerted_actors,population[2])
                    max_rockets=max(max_rockets,sample['rf_scene_rockets'][0])
                    max_grenades=max(max_grenades,sample.get('rf_scene_grenades',[0,0])[1])
                    grenade=sample.get('rf_scene_grenades',[0]*8)
                    max_grenade_bounces=max(max_grenade_bounces,grenade[2])
                    max_grenade_detonations=max(max_grenade_detonations,grenade[3])
                    max_terrain_edits=max(max_terrain_edits,sample.get('rf_scene_geomod',[0]*8)[7])
                    max_debris=max(max_debris,sample.get('rf_scene_debris',[0]*8)[0])
                    max_blast_kills=max(max_blast_kills,sample.get('rf_scene_rocket_blast',[0,0,0,0])[3])
                result['observed']={'distinct_eye_positions':len(eyes),'shots':max_shots,'weapon_switches':max_switches,
                    'enemy_shots':max_enemy_shots,'enemy_alerts':max_alerts,'deaths':max_deaths,'respawns':max_respawns,'alive_samples':alive_samples,'dead_samples':dead_samples,'max_live_actors':max_live_actors,'max_alerted_actors':max_alerted_actors}
                activity=sample.get('rf_scene_crash_activity')
                if activity:
                    result['observed'].update(live_input_frames=activity[0],dead_input_frames=activity[1],scene_starts=activity[2],total_deaths=activity[3])
                result['observed'].update(rocket_launches=max_rockets,grenade_releases=max_grenades,blast_kills=max_blast_kills)
                result['observed'].update(grenade_bounces=max_grenade_bounces,grenade_detonations=max_grenade_detonations,terrain_edits=max_terrain_edits,debris_spawned=max_debris)
                if args.loadout in (5,6):
                    result['requested_weapon_observed'] = (max_rockets if args.loadout==5 else max_grenades)>0
                result['combat_observed'] = max_enemy_shots > 0 and max_shots > 0
                result['multiple_enemy_alerts_observed'] = max_alerted_actors >= 2
                diagnostic=sample['rf_diagnostic']
                if diagnostic[2]&0x80000000:
                    result['status']='GUEST_ERROR'
                    # Routine gameplay runs retain application-owned telemetry
                    # already sampled above. Do not automatically expand a game
                    # error into CPU/register/instruction/stack inspection.
                    (run/'failure-report.json').write_text(json.dumps({
                        'kind':'application_telemetry',
                        'consistency':'Non-atomic sample; no additional failure-time reads.',
                        'xbe_sha256':result['xbe_sha256'],
                        'map_sha256':result['map_sha256'],
                        'sample':sample},indent=2))
                    break
                # Native terminal success validates the final scene's frame
                # count. Presentation counts reset on ordinary level reload;
                # replay consumption remains global across those reloads.
                if diagnostic[1]==9 and diagnostic[2]==5 and sample['rf_player_replay_diagnostic'][2]>=args.frames:
                    result['status']='COMPLETED';break
                if ram_reader:
                    if not pending_samples:time.sleep(15)
                else:time.sleep(args.poll_seconds)
        if result['status']=='TIMEOUT' and process.poll() is not None:
            result['status']='PROCESS_EXITED'
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
        if args.scenario:
            trace=result.get('last',{}).get('rf_scene_crash_scenario_trace',[0]*8)
            result['scenario_coverage']={
                'game_owned_phases_observed':trace[0] in (1,2,3,4) and trace[4]>=6 and trace[5]>=2,
                'repeated_player_fire':max_shots>=12,
                'repeated_enemy_fire':max_enemy_shots>=6,
                'simultaneous_enemy_alerts':max_alerted_actors>=2,
                'requested_explosive_repeated':args.loadout not in (5,6) or (max_rockets if args.loadout==5 else max_grenades)>=2,
            }
            # Coverage gates qualify this bounded encounter only, not stability.
            result['scenario_qualified']=result['status']=='COMPLETED' and all(result['scenario_coverage'].values())
        if run.exists():(run/'result.json').write_text(json.dumps(result,indent=2))
        lock.close()


if __name__=='__main__':main()
