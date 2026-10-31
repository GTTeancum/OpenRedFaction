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
from repository_runtime import confined_path, child_environment
from xemu_session_guard import require_no_project_xemu
from xemu_smoke import Monitor
from xemu_world_hdd import standalone

ROOT = Path(__file__).resolve().parents[1]


def static_world_bounds(level):
    """Conservative exterior guard, not proof of valid floor/room placement."""
    import io
    from inspect_levels import inspect
    from inspect_geometry import inspect as geometry
    meta = inspect(io.BytesIO(level), dict(offset=0, size=len(level), name='probe.rfl'))
    section = next(s for s in meta['sections'] if s['type']=='0x100')
    data = level[section['offset']+8:section['offset']+8+section['size']]
    info = geometry(data)
    vertices = list(struct.iter_unpack('<3f', data[info['vertices_offset']:info['vertices_offset']+info['vertices']*12]))
    if not vertices or not all(math.isfinite(v) for point in vertices for v in point):
        raise ValueError('Invalid static world bounds')
    return dict(minimum=[min(p[i] for p in vertices) for i in range(3)],
                maximum=[max(p[i] for p in vertices) for i in range(3)], margin=16.0)


def outside_world(sample, bounds, state):
    activity = sample.get('rf_scene_crash_activity', [0, 0, 0, 0])
    if not activity[0]:
        return None
    # Bounds belong to the authored level. More scene starts than death
    # reloads means an ordinary level exit loaded another level; latch it,
    # because a later death in that next level rebalances the counters.
    if len(activity) > 3 and activity[2] > activity[3] + 1:
        state['left_authored_level'] = True
    # A load (death recovery or quick-load) may resume another level's save.
    choice = sample.get('rf_scene_quickload_choice', [0]*8)
    if choice[0] and choice[3] != choice[4]:
        state['left_authored_level'] = True
    if state.get('left_authored_level'):
        return None
    eye = [struct.unpack('<f', struct.pack('<I', v))[0] for v in sample['rf_scene_gameplay_eye']]
    if any(not math.isfinite(v) or v < bounds['minimum'][i]-bounds['margin'] or
           v > bounds['maximum'][i]+bounds['margin'] for i,v in enumerate(eye)):
        return dict(eye=eye, bounds=bounds,
                    reason='Player eye outside conservative authored static geometry bounds; gameplay coverage invalid.')
    return None


def static_floor_below(level, position):
    """Conservative staged-start screen; not a body-fit or mover-support test."""
    import io
    from inspect_levels import inspect
    from inspect_geometry import inspect as geometry
    meta = inspect(io.BytesIO(level), dict(offset=0, size=len(level), name='probe.rfl'))
    section = next(s for s in meta['sections'] if s['type']=='0x100')
    data = level[section['offset']+8:section['offset']+8+section['size']]
    info = geometry(data)
    at = info['vertices_offset']
    vertices = list(struct.iter_unpack('<3f', data[at:at+info['vertices']*12]))
    at += info['vertices']*12
    count, = struct.unpack_from('<I', data, at); at += 4
    x,y,z = position
    hits = []
    for face in range(count):
        header = data[at:at+56]; at += 56
        normal_y, = struct.unpack_from('<f', header, 4)
        lightmap, = struct.unpack_from('<I', header, 20)
        portal,flags = struct.unpack_from('<II', header, 36)
        room,corners = struct.unpack_from('<II', header, 48)
        stride = 12 if lightmap==0xffffffff else 20
        points = [vertices[struct.unpack_from('<I',data,at+i*stride)[0]] for i in range(corners)]
        at += corners*stride
        if portal or flags&1 or normal_y<.5:
            continue
        a = points[0]
        for b,c in zip(points[1:-1],points[2:]):
            den = (b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2])
            if abs(den)<1e-8: continue
            u = ((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/den
            v = ((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/den
            if min(u,v,1-u-v)<-1e-5: continue
            floor = u*a[1]+v*b[1]+(1-u-v)*c[1]
            if .05 <= y-floor <= 3.0:
                hits.append(dict(face=face,room=room,height=floor,clearance=y-floor))
            break
    return max(hits,key=lambda h:h['height']) if hits else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--level', default='L1S1.rfl')
    parser.add_argument('--archive', default='levels1.vpp')
    parser.add_argument('--actor', type=int)
    parser.add_argument('--distance', type=float, help='Stage this many units in front of --actor, preserving all authored entities')
    parser.add_argument('--spawn-y', type=float, help='Override only the staged player-start height; requires --distance')
    parser.add_argument('--seed', type=int, default=1, help='Random input seed')
    parser.add_argument('--save-load-every', type=int, metavar='FRAMES', help='Alternate quick-save and quick-load every FRAMES replay frames (up to 64 actions)')
    parser.add_argument('--authored-source', type=int, help='ctf06 developer destruction source UID (requires --dev-room)')
    parser.add_argument('--dev-driller', action='store_true', help='ctf06 developer room with the Driller (authored source 148)')
    parser.add_argument('--dev-vehicle', type=int, choices=(1,2,3,4,5), help='Developer vehicle fixture: 1 Driller, 2 APC, 3 Jeep, 5 Fighter (ctf06); 4 submarine (L5S3)')
    parser.add_argument('--dev-npc', choices=tuple('23456789ABC'), help='Developer NPC fixture mode (3 grenade, 4 rocket, 5 riot shield, C shotgun, ...)')
    parser.add_argument('--world-arrays', action='store_true', help='Draw the retained world with draw arrays instead of inline indices')
    parser.add_argument('--load-only', action='store_true', help='With --save-load-every: save once, then only load')
    parser.add_argument('--quick-actions', type=lambda v: int(v,0), nargs=2, metavar=('SAVE','LOAD'), help='Replay frames that press quick-save and quick-load (0xffffffff = never)')
    parser.add_argument('--loadout', type=int, choices=(1,2,4,5,6,7,8), help='Diagnostic pistol/rifle/shotgun/rocket/grenade/sniper/railgun start; ordinary finite ammunition')
    parser.add_argument('--aim', action='store_true', help='Process-local bounded look input toward live hostile actors')
    parser.add_argument('--aim-actor', type=int, help='Prioritize this authored UID through bounded look input; nearest-hostile fallback after loss')
    parser.add_argument('--aim-point', type=float, nargs=3, metavar=('X','Y','Z'),
                        help='Process-local bounded look input toward an explicit world point')
    parser.add_argument('--scenario', choices=('combat','explosives','mobile','advance'), help='Game-owned live-tick phases; advance adds forward input to mobile strafing, explosives recovers the requested weapon through ordinary cycle inputs')
    parser.add_argument('--dev-room', action='store_true')
    parser.add_argument('--firearms', type=int, choices=(1,2,3,4))
    parser.add_argument('--tracer-hold', action='store_true', help='Player tracers stay where they start (capture aid)')
    parser.add_argument('--dev-slot', type=int, choices=range(18), help='Developer room starts holding this weapon slot (0 pistol .. 10 flamethrower)')
    parser.add_argument('--input', choices=('neutral','exercise','random','vehicle','vehicle-quick','vehicle-land','vehicle-stress','vehicle-hold','vehicle-cycle','vehicle-scuttle','vehicle-siege','driller-dig','combat','approach','approach-jump','scanner','live','fire','fire-once'), default='neutral')
    parser.add_argument('--frames', type=int, default=1800)
    parser.add_argument('--seconds', type=int, default=180)
    parser.add_argument('--log-protocol', choices=('qmp-telemetry','jkdf2-hmp'), default='jkdf2-hmp',
                        help='Default uses the unchanged OpenJKDF2 RAM log reader over HMP')
    parser.add_argument('--target', choices=('xemu','hardware'), default='xemu',
                        help='hardware stages the disc folder on an Xbox debug kit over xbdm')
    parser.add_argument('--xbox-host', default='10.0.0.231', help='Debug-kit address for --target hardware')
    parser.add_argument('--paced', action='store_true',
                        help='Consume replay input on the live 60 Hz frame clock with presentation skipping')
    parser.add_argument('--experiment', type=int, choices=range(8), default=0,
                        help='Bottleneck experiment bits: 1 issue no draws, 2 quarter-screen raster clip, 4 1x1 textures')
    parser.add_argument('--frame-rate', choices=('60','30','30_smooth','auto'),
                        help='write openrf.ini with this frame_rate (absent: no file, fixed 60)')
    parser.add_argument('--textures', choices=('compressed','original'), default='compressed',
                        help='original writes textures = original into openrf.ini (pack still staged)')
    parser.add_argument('--resolution', choices=('480','720'), default='480',
                        help='720: openrf.ini resolution = 720; xemu also gets an HDTV pack and 720p/480p enabled in its EEPROM')
    parser.add_argument('--color-depth', choices=('auto','32','16'), default='auto',
                        help='32/16 write color_depth into openrf.ini (auto: game default)')
    parser.add_argument('--shot-on-ir', action='store_true',
                        help='with --screenshots: capture when the infrared weapon screen shows a warm body')
    parser.add_argument('--screenshots', type=int, default=0,
                        help='xemu + qmp-telemetry only: N QMP screendumps spread through the replay')
    parser.add_argument('--render-sync', type=int, choices=range(4), default=0,
                        help='Renderer overlap bisect: 1 keep pbkit resets, 2 no fences/vertex halves, 3 both (old behaviour)')
    parser.add_argument('--no-audio', action='store_true', help='Leave the Xbox APU audio backend closed')
    parser.add_argument('--poll-seconds', type=float, default=1.0,
                        help='Application telemetry interval (0.3..5 seconds); larger intervals may miss short-lived states')
    args = parser.parse_args()
    if not 1 <= args.frames <= 240000 or not 1 <= args.seconds <= 4500:
        parser.error('Require 1..60000 frames and 1..600 seconds')
    if not .3 <= args.poll_seconds <= 5:
        parser.error('Require 0.3..5 seconds between telemetry samples')
    if args.dev_room and args.dev_vehicle == 4:
        if args.level != 'L5S3.rfl' or args.archive != 'levels1.vpp':
            parser.error('The developer submarine fixture uses L5S3.rfl from levels1.vpp')
    elif args.dev_room and (args.level not in ('ctf06.rfl','glass_house.rfl') or args.archive != 'levelsm.vpp'):
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
    emu = confined_path(ROOT, ROOT / 'local/runtime/xemu')
    packer = confined_path(ROOT, ROOT / 'local/runtime/extract-xiso.exe')
    for required in (packer, emu/'xemu.exe', emu/'MCPX/mcpx_1.0.bin',
                     emu/'BIOS/xbox-4627_debug.bin', emu/'EEPROM/eeprom.bin'):
        if args.target == 'xemu' and not confined_path(ROOT, required).is_file():
            parser.error('Missing repository-local runtime file: ' + str(required))
    lock.acquire()
    process = monitor = None
    ram_reader = None
    pending_samples = []
    seen_samples = set()
    ram_poll_index = 0
    run = ROOT / 'artifacts/crash-hunt' / (datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + args.level)
    for suffix in range(2, 100):
        if not run.exists(): break
        run = run.with_name(run.name.split('~')[0] + '~' + str(suffix))
    result = {'recipe': vars(args), 'input_recipe_version': 7, 'status': 'NOT_RUN'}
    eyes = set(); npc_positions = set(); npc_published_positions = set()
    npc_playback_hashes = set(); npc_pose_hashes = set()
    max_npc_active_clips = max_npc_animation_generation = 0
    max_shots = max_switches = 0
    max_enemy_shots = max_alerts = max_deaths = max_respawns = 0
    alive_samples = dead_samples = 0
    max_live_actors = max_alerted_actors = 0
    max_rockets = max_grenades = max_blast_kills = 0
    max_grenade_bounces = max_grenade_detonations = max_terrain_edits = max_debris = 0
    try:
        require_no_project_xemu(ROOT)
        disc = ROOT / 'build/xbox/crash-disc'
        base = ROOT / 'local/xemu-harness/pacing-base.qcow2'
        if args.target == 'xemu': standalone(base)
        # Cleanup may prune regenerable archives, so restore only this harness's
        # missing immutable inputs through hardlinks, never overwrite assets.
        for source in (ROOT / 'build/xbox/tester-disc').iterdir():
            if source.name in ('default.xbe','tester-profile.flag'): continue
            dest = disc / source.name
            if source.is_file() and not dest.exists(): dest.hardlink_to(source)
        # Cleanup also prunes the copied original archives; relink any missing.
        for name in ('tables','levels1','levels2','levels3','levelsm','maps1','maps2','maps3','maps4','maps_en','meshes','motions','audio','music','ui'):
            dest = disc / (name + '.vpp')
            if not dest.exists(): dest.hardlink_to(ROOT / 'Installed_Game' / (name + '.vpp'))
        # Generated DXT texture pack (tools/dds_pack.c); relink when rebuilt.
        pack = ROOT / 'build/data/rfdds.vpp'
        dest = disc / 'rfdds.vpp'
        if not pack.exists():
            # Cleanup prunes the pack; without it 720p textures do not fit.
            tool = ROOT / 'build/pc/Release/rf_dds_pack.exe'
            if not tool.is_file(): parser.error('Missing build/data/rfdds.vpp and build/pc/Release/rf_dds_pack.exe')
            pack.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run([str(tool), str(ROOT / 'Installed_Game'), str(pack)], check=True)
        if pack.exists() and (not dest.exists() or not dest.samefile(pack)):
            dest.unlink(missing_ok=True); dest.hardlink_to(pack)
        run = confined_path(ROOT, run)
        run.mkdir(parents=True)
        runtime_env = child_environment(ROOT, run)
        # Pin authored encounter membership; actor staging changes only the
        # player placement, never enemy count or original archive bytes.
        from build_fragment_platform_fixture import read_entry
        from xemu_turret_combat import entity_rows
        level_bytes = read_entry(ROOT/'Installed_Game'/args.archive,args.level)
        result['world_bounds'] = static_world_bounds(level_bytes)
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
        for name in ('campaign-actor.bin', 'campaign-level.bin', 'campaign-spawn.flag', 'player-replay.bin', 'dev-room.flag', 'firearms-test.flag', 'crash-loadout.bin', 'crash-aim.flag', 'crash-aim-point.bin', 'crash-aim-actor.bin', 'crash-scenario.flag', 'crash-ram-log.flag', 'crash-start.vpp', 'scene-fixture.vpp', 'crash-paced.flag', 'audio-output.flag', 'crash-experiment.bin', 'render-overlap.bin', 'campaign-quick-actions.bin', 'renderer-world-indices-off.flag', 'authored-source.bin', 'vehicle-test.flag', 'dev-npc.flag', 'crash-run-nonce.bin', 'openrf.ini', 'dev-slot.bin', 'tracer-hold.flag'):
            (disc / name).unlink(missing_ok=True)
        ini = []
        if args.frame_rate: ini += ['frame_rate = %s' % args.frame_rate, 'fallback = 30_smooth']
        if args.textures == 'original': ini.append('textures = original')
        if args.resolution == '720': ini.append('resolution = 720')
        if args.color_depth != 'auto': ini.append('color_depth = %s' % args.color_depth)
        if ini: (disc / 'openrf.ini').write_text('[video]\n' + '\n'.join(ini) + '\n')
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        run_nonce = int.from_bytes(os.urandom(4),'little') or 1
        (disc / 'crash-run-nonce.bin').write_bytes(run_nonce.to_bytes(4,'little'))
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
            support = static_floor_below(level_bytes, position)
            if support is None:
                raise ValueError('Staged start lacks nearby static floor support; choose a supported position before launching')
            result['staged_floor_support'] = support
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
        import random
        rng = random.Random(args.seed); random_state = {'until': 0}
        rows = []
        for frame in range(args.frames):
            move = [0., 0., 0.]; look = [0., 0.]; buttons = [0]*7
            if args.input == 'vehicle':
                # Ordinary boarding, driving, firing, seat switching and exit.
                # Based on check_jeep_replay; effects remain game-owned.
                phase = frame % 600
                buttons[2] = int(phase in (12, 540))
                if 40 <= phase < 180: move[2] = .65
                if 100 <= phase < 150: look[1] = .2
                buttons[3] = int(190 <= phase < 240 or 360 <= phase < 420)
                buttons[6] = int(250 <= phase < 280 or 430 <= phase < 450)
                buttons[5] = int(phase in (320, 490))
            elif args.input == 'vehicle-quick':
                # Short ordinary lifecycle for lethal authored encounters:
                # board, move/turn, use primary fire, then exit via Use.
                # Secondary weapons remain in the longer vehicle sequence;
                # close APC rockets can kill the occupant before an exit.
                phase = frame % 180
                buttons[2] = int(phase in (12, 125))
                if 35 <= phase < 75: move[2] = .65
                if 55 <= phase < 75: look[1] = .2
                buttons[3] = int(80 <= phase < 110)
            elif args.input == 'vehicle-land':
                # Ordinary Fighter landing attempt before a voluntary exit.
                phase = frame % 540
                buttons[2] = int(phase in (12, 430))
                if 35 <= phase < 360:
                    move[2] = .35
                    buttons[0] = 1  # crouch maps to controlled descent
                if 80 <= phase < 120: look[1] = .15
                buttons[3] = int(130 <= phase < 175)
            elif args.input == 'vehicle-stress':
                # Remain occupied under authored pressure while continuously
                # driving, turning and exercising both fire controls.
                phase = frame % 360
                buttons[2] = int(frame == 12)
                if frame >= 35: move[2] = .7
                if 60 <= phase < 150: look[1] = .2
                elif 180 <= phase < 270: look[1] = -.2
                buttons[3] = int(90 <= phase < 160 or 210 <= phase < 280)
                buttons[6] = int(170 <= phase < 195 or 290 <= phase < 315)
            elif args.input == 'vehicle-hold':
                # DEV vehicle fixtures start 8.5 units away facing the host:
                # walk up, board once, then stay occupied under attack while
                # driving, turning and using both fire controls. The Fighter
                # and submarine fixtures are within reach, so they board at once.
                board = 12 if args.dev_vehicle in (4, 5) else 110
                if board == 110 and 20 <= frame < 100: move[2] = .65
                buttons[2] = int(frame == board)
                if frame >= board+50:
                    phase = (frame-board-50) % 360
                    move[2] = .5 if phase < 240 else -.4
                    if 60 <= phase < 150: look[1] = .2
                    elif 180 <= phase < 270: look[1] = -.2
                    buttons[3] = int(90 <= phase < 160 or 210 <= phase < 280)
                    buttons[6] = int(170 <= phase < 195 or 290 <= phase < 315)
            elif args.input == 'vehicle-cycle':
                # DEV vehicle fixtures: walk up, board, drive/turn with both
                # fire controls (Jeep: via the gunner seat), exit with Use, board again.
                board = 12 if args.dev_vehicle in (4, 5) else 110
                if board == 110 and 20 <= frame < 100: move[2] = .65
                if frame >= board:
                    phase = (frame-board) % 720
                    buttons[2] = int(phase in (0, 560))
                    if 50 <= phase < 500:
                        # Parked 250..470: the Jeep seat switch needs a stop.
                        if not 250 <= phase < 470: move[2] = .5 if phase < 200 else -.4
                        if 120 <= phase < 200: look[1] = .2
                        elif 300 <= phase < 380: look[1] = -.2
                        buttons[3] = int(150 <= phase < 220 or 330 <= phase < 400)
                        buttons[6] = int(240 <= phase < 265 or 420 <= phase < 445)
                        # Jeep: move to the gunner seat for the second burst and back.
                        buttons[5] = int(phase in (300, 450))
            elif args.input == 'vehicle-scuttle':
                # Occupied-hull destruction attempt: board at once, pitch the
                # nose down, then drive hard and keep firing at close range.
                buttons[2] = int(frame == 12)
                if frame >= 60:
                    phase = (frame-60) % 240
                    if phase < 30: look[0] = .25
                    move[2] = 1. if phase < 180 else -.6
                    buttons[3] = int(frame % 20 < 10)
            elif args.input == 'vehicle-siege':
                # On-foot destruction of an empty DEV vehicle: back away from
                # the host the fixture faces, cycle to the rocket launcher
                # (slot 4 of 5), fire once a second, refill with Use+Reload.
                if 20 <= frame < 80 and args.dev_vehicle != 4: move[2] = -.65
                # Submarine fixture: one more cycle reaches the Grenade (slot 5 of 6),
                # the explosive that fires underwater (weapons.tbl flag 0x200).
                buttons[5] = int(frame in ((100, 120, 140, 160, 180) if args.dev_vehicle == 4 else (100, 120, 140, 160)))
                # (Thrown grenades only reached the submarine hull from beside it:
                # use --aim-point at the hull; the player's own blasts can kill them.)
                if frame >= 200:
                    phase = (frame-200) % 240
                    buttons[3] = int(phase % 60 == 0 and phase < 200)
                    if 220 <= phase < 224: buttons[2] = buttons[4] = 1
            elif args.input == 'driller-dig':
                # ctf06 Driller fixture: walk up facing it, one Use to board,
                # then drive forward with the drill held, sweeping slowly.
                if 20 <= frame < 100: move[2] = .65
                buttons[2] = int(frame == 110)
                if frame >= 160:
                    phase = (frame-160) % 480
                    move[2] = .5 if phase < 360 else -.4
                    if 120 <= phase < 180: look[1] = .1
                    elif 300 <= phase < 360: look[1] = -.1
                    buttons[3] = int(phase < 360)
            elif args.input in ('approach','approach-jump'):
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
            elif args.input == 'random' and frame >= 60:
                # Seeded button mashing: sticks held for random spans, random
                # edges on every gameplay button. Pause/menu stay untouched.
                if frame == 60 or frame >= random_state['until']:
                    random_state['until'] = frame + rng.randint(6, 90)
                    random_state['move'] = [rng.choice((0.,0.,rng.uniform(-1,1))) for _ in range(3)]
                    random_state['move'][1] = 0.
                    random_state['look'] = [rng.choice((0.,rng.uniform(-.4,.4))) for _ in range(2)]
                    random_state['held'] = [int(rng.random() < p) for p in (.1,.05,.03,.35,.03,.04,.15)]
                move = list(random_state['move']); look = list(random_state['look'])
                buttons = [h if rng.random() < .85 else 0 for h in random_state['held']]
            elif args.input == 'scanner':
                # Rail Driver scanner: one alternate-fire edge turns it on,
                # then hold still (aim modes still steer the view).
                buttons[6] = int(frame == 90)
            elif args.input == 'fire-once':
                # One shot after the draw animation has settled (capture aid).
                buttons[3] = int(300 <= frame < 306)
            elif args.input == 'fire':
                # Held weapon only: repeated fire edges, no movement or switching.
                buttons[3] = int(frame >= 60 and frame % 12 < 6)
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
        # live: no replay file, so the title reads the console controller (unpaced replay off).
        if args.input != 'live': (disc / 'player-replay.bin').write_bytes(replay)
        result['replay_sha256'] = hashlib.sha256(replay).hexdigest()
        if args.dev_room: (disc / 'dev-room.flag').write_bytes(b'1')
        if args.authored_source is not None: (disc / 'authored-source.bin').write_bytes(struct.pack('<I', args.authored_source))
        if args.dev_driller: (disc / 'vehicle-test.flag').write_text('1')
        if args.dev_vehicle: (disc / 'vehicle-test.flag').write_text(str(args.dev_vehicle))
        if args.dev_npc: (disc / 'dev-npc.flag').write_text(args.dev_npc)
        if args.firearms: (disc / 'firearms-test.flag').write_text(str(args.firearms))
        if args.tracer_hold: (disc / 'tracer-hold.flag').write_bytes(b'')
        if args.dev_slot is not None: (disc / 'dev-slot.bin').write_bytes(struct.pack('<I', args.dev_slot + 1))
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
            (disc / 'crash-scenario.flag').write_bytes({'combat':b'', 'explosives':b'2', 'mobile':b'3', 'advance':b'4'}[args.scenario])
        if args.log_protocol=='jkdf2-hmp' or args.target=='hardware':
            (disc / 'crash-ram-log.flag').write_bytes(b'')
        if args.paced and args.input != 'live': (disc / 'crash-paced.flag').write_bytes(b'')
        if args.render_sync: (disc / 'render-overlap.bin').write_bytes(struct.pack('<I',args.render_sync))
        if args.experiment: (disc / 'crash-experiment.bin').write_bytes(struct.pack('<I',args.experiment))
        if args.world_arrays: (disc / 'renderer-world-indices-off.flag').write_bytes(b'')
        if args.quick_actions: (disc / 'campaign-quick-actions.bin').write_bytes(struct.pack('<2I',*args.quick_actions))
        if args.save_load_every:
            pairs = [(f, (1 if i == 0 else 2) if args.load_only else 1 + (i % 2)) for i, f in enumerate(range(args.save_load_every, args.frames, args.save_load_every))][:64]
            (disc / 'campaign-quick-actions.bin').write_bytes(b'RFQA' + struct.pack('<I', len(pairs)) + b''.join(struct.pack('<2I', *p) for p in pairs))
            result['quick_actions'] = pairs
        if not args.no_audio: (disc / 'audio-output.flag').write_bytes(b'')
        result['xbox_audio_backend'] = not args.no_audio
        if args.target == 'hardware':
            import xbox_hw
            remote = 'E:' + chr(92) + 'RFCrash' + chr(92)
            try:
                sent = xbox_hw.sync_disc(args.xbox_host, disc, remote, ROOT/'build/xbox/hardware-sync.json')
            except RuntimeError as e:
                # A title still running from the last session holds its run files open
                # (xbdm 413); reboot to the dashboard and copy once more.
                if '413' not in str(e): raise
                xbox_hw.reboot(args.xbox_host)
                sent = xbox_hw.sync_disc(args.xbox_host, disc, remote, ROOT/'build/xbox/hardware-sync.json')
            result['hardware'] = {'host': args.xbox_host, 'folder': remote, 'bytes_sent': sent}
            reader_path=ROOT/'tools/vendor/openjkdf2/poll_xemu_ram_log.py'
            spec=importlib.util.spec_from_file_location('jkdf2_ram_log',reader_path)
            vendored=importlib.util.module_from_spec(spec);spec.loader.exec_module(vendored)
            poll_map=run/'poll-build.map'
            poll_map.write_text(re.sub(r'\b00000000([0-9a-fA-F]{8})\b',r'\1',mapping))
            ram_symbols=vendored.resolve_symbols(str(poll_map),str(disc/'default.xbe'))
            ram_args=None
            ram_reader=xbox_hw.HardwareReader(args.xbox_host,vendored.decode_mirror,vendored.MIRROR_BYTES,
                                              (vendored.MAGIC0,vendored.MAGIC1))
            result['ram_poll_interval_seconds']=5
            xbox_hw.launch(args.xbox_host, remote + 'default.xbe').close()
            class Running:
                pid = None
                def poll(self): return None
            process = Running()
            port = None
            deadline = time.monotonic() + args.seconds
            result['status'] = 'TIMEOUT'
            time.sleep(5)
        else:
          iso = ROOT / 'build/xbox/redfaction-crash.iso'
          iso.unlink(missing_ok=True)
          with (run / 'pack.log').open('wb') as log:
              subprocess.run([str(packer),'-c',str(disc),str(iso)],cwd=run,env=runtime_env,check=True,stdout=log,stderr=subprocess.STDOUT)
          shutil.copyfile(emu/'EEPROM/eeprom.bin',run/'eeprom.bin')
          if args.resolution == '720':
              # Dashboard video settings (user section, 0x94): 480p|720p|60Hz,
              # then the user-section checksum at 0x60 over 0x64..0xbf.
              e=bytearray((run/'eeprom.bin').read_bytes())
              e[0x94:0x98]=struct.pack('<I',0x00080000|0x00020000|0x00400000)
              high=low=0
              for i in range(0x64,0x64+0x5c,4):
                  v=struct.unpack('<I',e[i:i+4])[0];total=((high<<32)|low)+v;high=(total>>32)&0xffffffff;low=total&0xffffffff
              e[0x60:0x64]=struct.pack('<I',(~(high+low))&0xffffffff)
              (run/'eeprom.bin').write_bytes(bytes(e))
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
[sys]
avpack = 'hdtv'
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
          counts={'rf_diagnostic':58,'rf_scene_enemy_failure':6,'rf_scene_tactical_failure':3,'rf_scene_enemy_combat':8,'rf_scene_combat_population':4,'rf_scene_enemy_awareness':8,'rf_scene_npc_registration':6,'rf_scene_player_ammo':8,'rf_scene_rockets':8,'rf_scene_rocket_blast':8,'rf_scene_grenades':8,'rf_scene_geomod':8,'rf_scene_debris':8,'rf_scene_profile_stage':2,'rf_scene_presentation_failure':8,'rf_scene_late_load':8,'rf_xbox_renderer_stage':4,'rf_xbox_draw_audit':4,'rf_xbox_texture_budget':12,'rf_xbox_load_stage':1,'rf_scene_campaign_load_stage':1,'rf_scene_campaign_load_detail':1,'rf_scene_npc_body_admission':4,'rf_scene_npc_body_failure':2,'rf_scene_patrol_failure':3,'rf_player_replay_diagnostic':4,'rf_scene_actor_frame_count':1,'rf_scene_player_life':8,'rf_scene_crash_activity':4,'rf_scene_crash_aim_trace':4,'rf_scene_crash_enemy_state':12,'rf_scene_crash_npc_motion':12,'rf_scene_crash_npc_progress':12,'rf_scene_npc_playback':7,'rf_scene_npc_eyes':4,'rf_scene_enemy_aim':4,'rf_scene_gameplay_eye':3,'rf_scene_combat':8,'rf_scene_weapon_selection':8,'rf_preview_failure':8,'rf_scene_world_texture_budget':4,'rf_xbox_screen':4,'rf_xbox_video':4,'rf_xbox_frame_failure':3}
          symbols={}
          counts['rf_scene_crash_scenario_trace']=8
          counts['rf_scene_crash_visibility']=4
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
          with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
              process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,env=runtime_env)
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
        poll_wait = 5 if args.target == 'hardware' else 15
        shots_taken = 0
        last_shot_input = -1000
        stall = [None, time.monotonic()]
        outside_streak = 0
        with (run/'telemetry.jsonl').open('w',buffering=1) as log:
            while time.monotonic()<deadline and process.poll() is None:
                try:
                    if ram_reader:
                        if not pending_samples:
                            delta,header,ram_text=ram_reader.poll_port(port,ram_symbols,ram_args)
                            ram_poll_index+=1
                            (run/f'ram_poll_{ram_poll_index:04d}.txt').write_text(ram_text)
                            result['ram_header']=header;result['ram_physical_delta']=delta
                            if args.target=='hardware':
                                # A halted title (exception/break) stops log writes while
                                # memory stays readable; report it instead of waiting out.
                                if header['writes']!=stall[0]: stall[:]=[header['writes'],time.monotonic()]
                                elif time.monotonic()-stall[1]>180:
                                    result['status']='STALLED';result['stalled_header']=header;break
                            for line in ram_text.splitlines():
                                if not line.startswith('RFLOG '):continue
                                try:value=json.loads(line[6:])
                                except json.JSONDecodeError:continue
                                # A torn mirror read can parse yet omit fields; wait for a whole sample.
                                if 'rf_player_replay_diagnostic' not in value or 'rf_diagnostic' not in value:continue
                                if value.get('rf_xbox_run_nonce',[run_nonce])[0]!=run_nonce:continue
                                key=(value['rf_player_replay_diagnostic'][2],value['rf_diagnostic'][2],
                                     tuple(value.get('rf_xbox_startup_trace',[])))
                                if key not in seen_samples:
                                    seen_samples.add(key);pending_samples.append(value)
                        if not pending_samples:
                            time.sleep(poll_wait);continue
                        sample=pending_samples.pop(0)
                        pages=sample['rf_diagnostic'][3]
                        result['memory']={'source':'game MmQueryStatistics','base-memory':pages*4096}
                        if pages!=16384:raise ValueError('Game log did not confirm stock64')
                    else:sample={name:words(monitor,addr,count) for name,(addr,count) in symbols.items()}
                except (RuntimeError,OSError) as exc:
                    result['last_read_error']=str(exc);time.sleep(15 if ram_reader else .2);continue
                sample['elapsed_seconds']=round(args.seconds-(deadline-time.monotonic()),3)
                log.write(json.dumps(sample)+'\n');result['last']=sample
                # Native emulator capture (QMP screendump), evenly through the replay.
                if args.screenshots and shots_taken<args.screenshots and (args.target=='hardware' or (monitor and not ram_reader)):
                    consumed=(sample.get('rf_player_replay_diagnostic') or [0,0,0])[2]
                    ir=(sample.get('rf_scene_weapon_ir') or [0,0])[1]
                    # --shot-on-ir: capture only while the weapon screen shows heat,
                    # at least 120 inputs apart; otherwise evenly through the replay.
                    if (ir>0 and consumed>=last_shot_input+120) if args.shot_on_ir else consumed>=args.frames*(shots_taken+1)//(args.screenshots+1):
                        last_shot_input=consumed
                        try:
                            if args.target=='hardware':
                                name=f'shot_{shots_taken:02d}.png'
                                screen=sample.get('rf_xbox_screen') or [640,480]
                                hazard=sample.get('rf_xbox_scanout_hazard') or [0,0,0]
                                info=xbox_hw.save_framebuffer(args.xbox_host,str(run/name),screen[0] or 640,screen[1] or 480,hazard[2],
                                    16 if len(screen)>3 and (screen[3]>>8)==16 else 32)
                            else:
                                name=f'shot_{shots_taken:02d}.ppm';info=None
                                monitor.command('screendump',{'filename':str(run/name)})
                            result.setdefault('screenshots',[]).append({'file':name,'input':consumed,'screen':sample.get('rf_xbox_screen'),'xbdm':info})
                        except (RuntimeError,OSError) as exc: result['screenshot_error']=str(exc)
                        shots_taken+=1
                if sample['rf_player_replay_diagnostic'][2]:
                    if sample['rf_scene_player_life'][2]: dead_samples += 1
                    else: alive_samples += 1
                    eyes.add(tuple(sample['rf_scene_gameplay_eye']))
                    motion=sample.get('rf_scene_crash_npc_motion',[0]*12)
                    if motion[0]:
                        npc_positions.add(tuple(motion[2:5]))
                        npc_published_positions.add(tuple(motion[5:8]))
                        npc_playback_hashes.add(motion[8])
                        npc_pose_hashes.add(motion[9])
                        max_npc_active_clips=max(max_npc_active_clips,motion[10])
                        max_npc_animation_generation=max(max_npc_animation_generation,motion[11])
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
                result['observed'].update(tracked_npc_uid=sample.get('rf_scene_crash_npc_motion',[0])[0],
                    distinct_npc_body_positions=len(npc_positions),distinct_npc_published_positions=len(npc_published_positions),
                    distinct_npc_playback_hashes=len(npc_playback_hashes),distinct_npc_pose_hashes=len(npc_pose_hashes),
                    max_npc_active_clips=max_npc_active_clips,max_npc_animation_generation=max_npc_animation_generation)
                progress=sample.get('rf_scene_crash_npc_progress',[0]*12)
                result['observed'].update(tracked_npc_ticks=progress[1],tracked_npc_body_changes=progress[2],
                    tracked_npc_published_changes=progress[3],tracked_npc_playback_changes=progress[4],
                    tracked_npc_pose_changes=progress[5],tracked_npc_live_frames=progress[8],
                    tracked_npc_alert_frames=progress[9],tracked_npc_health_bits=progress[10])
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
                visibility = sample.get('rf_scene_crash_visibility', [0]*4)
                if visibility[2]:
                    result['status']='INPUT_QUERY_ERROR'
                    result['input_query_error']=visibility
                    break
                escaped = outside_world(sample, result['world_bounds'], result)
                # A torn/stale RAM-mirror line from a previous session can show
                # another level's eye once; require two consecutive escapes.
                outside_streak = outside_streak + 1 if escaped else 0
                if escaped and outside_streak >= 2:
                    result['status']='OUTSIDE_WORLD'
                    result['outside_world']=escaped
                    (run/'outside-world.json').write_text(json.dumps(dict(evidence=escaped,sample=sample),indent=2))
                    break
                # Native terminal success validates the final scene's frame
                # count. Presentation counts reset on ordinary level reload;
                # replay consumption remains global across those reloads.
                if diagnostic[1]==9 and diagnostic[2]==5 and sample['rf_player_replay_diagnostic'][2]>=args.frames:
                    result['status']='COMPLETED';break
                if ram_reader:
                    if not pending_samples:time.sleep(poll_wait)
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
        if process and args.target == 'hardware':
            import xbox_hw
            xbox_hw.reboot(args.xbox_host)
        elif process:
            stop_owned_process(process)
            result['exit_code']=process.poll()
        if args.scenario:
            trace=result.get('last',{}).get('rf_scene_crash_scenario_trace',[0]*8)
            result['scenario_coverage']={
                'game_owned_phases_observed':trace[0] in (1,2,3,4,5) and trace[4]>=6 and trace[5]>=2,
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
