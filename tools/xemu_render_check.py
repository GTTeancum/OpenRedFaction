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
from verify_water_xbox import verify as verify_water_scenario
from xemu_texture_audit import capture as capture_texture, capture_atlas
from xemu_draw_audit import capture as capture_draws


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--npc-projectile-test', action='store_true', help='Rubble-cover fixture followed by a rocket striking the guard')
    parser.add_argument('--firearms-test', type=int, choices=(1,2,3,4), help='DEV remaining firearms: MP/HMG/precision/undercover')
    parser.add_argument('--fusion-test', action='store_true', help='DEV Fusion launcher with enemy-free weapon fixture')
    parser.add_argument('--player-shield-test', action='store_true', help='DEV player holding authored first-person shield')
    parser.add_argument('--npc-shield-test', action='store_true', help='DEV miner with authored riot shield stance')
    parser.add_argument('--npc-rocket-test', action='store_true', help='DEV guard with finite live rockets')
    parser.add_argument('--npc-grenade-test', action='store_true', help='DEV guard with three finite ballistic grenades')
    parser.add_argument('--npc-rubble-test', action='store_true', help='Opt-in armed NPC versus live extracted held cover')
    parser.add_argument('--ceiling-platform-test', action='store_true', help='Tall lift into ceiling: energy regression only; crush resolution remains open')
    parser.add_argument('--lift-platform-test', action='store_true', help='Raise developer platform0.5 units with rubble; implies --fragment-platform-test')
    parser.add_argument('--tip-platform-test', action='store_true', help='Tip the explicit platform90 degrees; implies --fragment-platform-test')
    parser.add_argument('--fragment-platform-test', action='store_true', help='Generated CTF06 platform with ordinary rocket rubble and support withdrawal; no saves')
    parser.add_argument('--fragment-contact-test', action='store_true', help='Run isolated narrow static/mover contact fixtures and compare64 guest words')
    parser.add_argument('--moving-support-test', action='store_true', help='Process-local saved rubble lift/stop/retire fixture')
    parser.add_argument('--release-support-test', action='store_true', help='Lift saved support then release to ordinary debris gravity/contact')
    parser.add_argument('--rotate-support-test', action='store_true', help='Release lifted support with a single angular impulse')
    parser.add_argument('--tip-support-test', action='store_true', help='Stronger tipping impulse and overlap regression')
    parser.add_argument('--expanded-geomod', action='store_true', help='Opt-in matching sixteen-cut PC/NXDK profile on stock64MiB')
    parser.add_argument('--terrain-texture-audit', action='store_true', help='Read live Xbox substrate texture bytes and compare the PC owner')
    parser.add_argument('--terrain-atlas-audit', action='store_true', help='Compare live generated atlas bytes for a settled checkpoint with neutral input')
    parser.add_argument('--terrain-draw-audit', action='store_true', help='Read submitted cap draw commands; implies settled atlas/texture audits')
    parser.add_argument('--terrain-map-limit',type=int,help='Explicit DEV fault injection: maximum new-map admission count')
    parser.add_argument('--terrain-map-limit-until',type=int,default=0xffffffff,help='Frame when injected map limit expires')
    parser.add_argument('--cpu-exceptions', action='store_true', help='Retain QEMU exception/reset diagnostics for guest crash analysis')
    parser.add_argument('--water-test', action='store_true', help='Authored dm03 water gameplay with DEV weapon supply')
    parser.add_argument('--swim-test', action='store_true', help='Authored L2S3 deep-pool movement fixture')
    parser.add_argument('--lava-test', choices=('wet','dry'), help='Authored L5S2 lava exposure or same-room dry control')
    parser.add_argument('--capture-ripple', action='store_true', help='Capture ordinary ripple vertices without injecting a fixture')
    parser.add_argument('--debris-player-test', action='store_true', help='Explicit scene damage fixture, not an ordinary fragment trajectory')
    parser.add_argument('--ripple-test', action='store_true', help='DEV render-only ripple fixture; no liquid collision claim')
    parser.add_argument('--authored-sources', type=int, choices=(1,2,3), default=1, help='Retain one/two sources, or beam95/98 with both posts; collections require player checkpoint mode')
    parser.add_argument('--authored-source', type=int, choices=(66,75,79,92,93,94,95,96,97,98,99,103,108), help='Select one ctf06 developer destruction source on both platforms')
    parser.add_argument('--dev-room', action='store_true', help='Supply supported weapons in Glass House or the authored ctf06 post test')
    parser.add_argument('--player-checkpoint', action='store_true', help='Opt-in RFCP player plus destruction checkpoint mode')
    parser.add_argument('--shallow-oblique', action='store_true', help='Use an oblique second shallow-region limit')
    parser.add_argument('--shallow-two-limits', action='store_true', help='Use two intersecting authored shallow-region fixtures')
    parser.add_argument('--cavity-seam-test', action='store_true', help='Explicit source66 floor-seam hardness fixture')
    parser.add_argument('--shallow-fixture', action='store_true', help='DEV depth.75 authored-region fixture shared with PC')
    parser.add_argument('--geomod-checkpoint-in', type=Path, help='Load a DEV destruction checkpoint before playback')
    parser.add_argument('--geomod-checkpoint-out', action='store_true', help='Capture bounded PC/Xbox destruction checkpoints and compare bytes')
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
    if sum((args.tip_platform_test,args.lift_platform_test,args.ceiling_platform_test))>1:parser.error('Choose one platform motion')
    if args.tip_platform_test or args.lift_platform_test or args.ceiling_platform_test:args.fragment_platform_test=True
    if args.fragment_platform_test and not (args.dev_room and args.spawn and args.level=='ctf06.rfl' and args.archive=='levelsm.vpp' and args.authored_source==108 and args.authored_sources==3 and args.input and not (args.player_checkpoint or args.geomod_checkpoint_in or args.geomod_checkpoint_out)):
        parser.error('--fragment-platform-test requires source108/three-source CTF06 DEV rocket input without checkpoints')
    if args.fragment_contact_test and not args.dev_room:parser.error('--fragment-contact-test requires --dev-room')
    if args.npc_projectile_test:args.npc_rubble_test=True
    if args.firearms_test and (not args.dev_room or args.level!='ctf06.rfl' or args.fusion_test or args.player_shield_test or args.npc_shield_test or args.npc_rocket_test or args.npc_grenade_test or args.npc_rubble_test or args.player_checkpoint):parser.error('--firearms-test requires enemy-free ctf06 DEV without other fixtures/checkpoints')
    if args.fusion_test and (not args.dev_room or args.level!='ctf06.rfl' or args.player_shield_test or args.npc_shield_test or args.npc_rocket_test or args.npc_grenade_test or args.npc_rubble_test or args.player_checkpoint):parser.error('--fusion-test requires enemy-free ctf06 DEV without other fixtures/checkpoints')
    if args.player_shield_test and (not args.dev_room or args.level!='ctf06.rfl' or args.npc_shield_test or args.npc_rocket_test or args.npc_grenade_test or args.npc_rubble_test or args.player_checkpoint):parser.error('--player-shield-test requires ctf06 DEV without other NPC fixtures/checkpoints')
    if args.npc_shield_test and (not args.dev_room or args.level!='ctf06.rfl' or args.npc_rocket_test or args.npc_grenade_test or args.npc_rubble_test or args.player_checkpoint):parser.error('--npc-shield-test requires ctf06 DEV without other NPC fixtures/checkpoints')
    if args.npc_rocket_test and (not args.dev_room or args.level!='ctf06.rfl' or args.npc_grenade_test or args.npc_rubble_test or args.player_checkpoint):parser.error('--npc-rocket-test requires ctf06 DEV without other NPC fixtures/checkpoints')
    if args.npc_grenade_test and (not args.dev_room or args.level!='ctf06.rfl' or args.npc_rubble_test or args.player_checkpoint):parser.error('--npc-grenade-test requires ctf06 DEV without rubble fixture/checkpoints')
    if args.npc_rubble_test and (not args.dev_room or args.level!='ctf06.rfl' or args.player_checkpoint or args.geomod_checkpoint_in):
        parser.error('--npc-rubble-test requires ctf06 DEV room with live extraction, no player checkpoint')
    if args.tip_support_test:args.rotate_support_test=True
    if args.release_support_test or args.rotate_support_test:args.moving_support_test=True
    if args.moving_support_test and not (args.dev_room and args.player_checkpoint and args.geomod_checkpoint_in):
        parser.error('--moving-support-test requires a saved DEV player checkpoint')
    if args.terrain_draw_audit:args.terrain_atlas_audit=args.terrain_texture_audit=True
    if args.terrain_texture_audit and not args.dev_room:parser.error('--terrain-texture-audit requires --dev-room')
    if args.terrain_atlas_audit and (not args.dev_room or not args.geomod_checkpoint_in):
        parser.error('--terrain-atlas-audit requires a settled DEV checkpoint')
    if args.terrain_map_limit is not None and (not args.dev_room or not 1<=args.terrain_map_limit<=(2048 if args.expanded_geomod else 1024) or not 1<=args.terrain_map_limit_until<=0xffffffff):
        parser.error('Map fault injection requires DEV mode and valid map/frame limits')
    if args.authored_source is not None and (not args.dev_room or args.level!='ctf06.rfl'):
        parser.error('--authored-source requires --dev-room --level ctf06.rfl')
    if args.authored_source in (66,75,79,99,103) and args.authored_sources!=1:
        parser.error('Cavity and guarded side-post selectors require a single source')
    if args.authored_sources>1:
        if not args.dev_room or args.level!='ctf06.rfl':parser.error('Source collections require ctf06 DEV room')
        if (args.geomod_checkpoint_in or args.geomod_checkpoint_out) and not args.player_checkpoint:
            parser.error('Collection checkpoints require --player-checkpoint')
    if args.authored_sources==3 and args.authored_source not in (92,95,98,108):
        parser.error('Three sources require beam --authored-source 92, 95, 98 or 108')
    if args.player_checkpoint and not args.dev_room:parser.error('--player-checkpoint requires --dev-room')
    if args.lava_test and (args.swim_test or args.water_test or args.dev_room or not args.spawn or args.level!='L5S2.rfl' or args.archive!='levels1.vpp'):
        parser.error('--lava-test requires --spawn --level L5S2.rfl --archive levels1.vpp without other placement fixtures')
    liquid_mode=2 if args.lava_test=='wet' else 3 if args.lava_test=='dry' else 1 if args.swim_test else 0
    if args.swim_test and (args.water_test or args.dev_room or not args.spawn or args.level!='L2S3.rfl' or args.archive!='levels1.vpp'):
        parser.error('--swim-test requires --spawn --level L2S3.rfl --archive levels1.vpp without other placement fixtures')
    if args.water_test and (args.dev_room or args.level!='dm03.rfl' or args.archive!='levelsm.vpp' or not args.spawn):
        parser.error('--water-test requires --spawn --level dm03.rfl --archive levelsm.vpp without --dev-room')
    if args.dev_room and (args.level not in ('glass_house.rfl','ctf06.rfl') or args.archive != 'levelsm.vpp' or not args.spawn):
        parser.error('Developer room requires --spawn --level glass_house.rfl or ctf06.rfl --archive levelsm.vpp')
    if args.debris_player_test and not args.dev_room:parser.error('--debris-player-test requires --dev-room')
    if args.ripple_test and not args.dev_room:parser.error('--ripple-test requires --dev-room')
    if args.shallow_oblique:args.shallow_two_limits=True
    if args.cavity_seam_test and (not args.dev_room or args.authored_source!=66):
        parser.error('--cavity-seam-test requires source66 developer mode')
    if args.shallow_two_limits:args.shallow_fixture=True
    if args.shallow_fixture and not args.dev_room:
        parser.error('--shallow-fixture requires --dev-room')
    if args.terrain_test_light and not args.dev_room:
        parser.error('--terrain-test-light requires --dev-room')
    if (args.geomod_checkpoint_in or args.geomod_checkpoint_out) and not args.dev_room:
        parser.error('GeoMod checkpoints require --dev-room')
    checkpoint_limit = 262144 if args.expanded_geomod else 110524
    checkpoint = args.geomod_checkpoint_in is not None or args.geomod_checkpoint_out
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
    if args.npc_rubble_test and (args.frames<962 or not args.input or args.authored_source!=95 or args.authored_sources!=3):
        parser.error('--npc-rubble-test requires the962-frame live extraction replay and authored sources95/94/93')
    if args.moving_support_test and (args.frames < 242 or any(payload[8:] if payload[:4] in (b'RFI2',b'RFI3',b'RFI4',b'RFI5',b'RFI6') else payload)):
        parser.error('--moving-support-test requires at least242 neutral input records')
    if args.terrain_atlas_audit and any(payload[8:] if payload[:4] in (b'RFI2',b'RFI3',b'RFI4',b'RFI5',b'RFI6') else payload):
        parser.error('--terrain-atlas-audit requires neutral replay input')
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
    pc_build = 'build/pc-expanded' if args.expanded_geomod else 'build/pc'
    disc = root / 'build/xbox/disc'
    run = root / 'artifacts/xemu' / ('render-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    run.mkdir(parents=True)
    print('Run:', run, flush=True)
    report = dict(result='FAIL', frames=args.frames, actor=None if args.item_uid or args.spawn else args.actor, level=args.level, archive=args.archive, goal_uid=args.goal_uid, item_uid=args.item_uid, model_culling=args.culled, command_batching=not args.unbatched, world_grouping=not args.unsorted,
        input_sha256=hashlib.sha256(payload).hexdigest(), setup_uids=args.setup_uid, trigger_start_uid=args.trigger_start_uid, exit_start_uid=args.exit_start_uid, exit_uid=args.exit_uid, return_exit_uid=args.return_exit_uid,
        scope='Authored section, player spawn or staged actor/pickup camera, process-local replay/setup commands, native framebuffer, '
              'phase timings and selected PC gameplay-state checks. No full campaign/parity claim.', samples=[])
    report['fragment_contact_test']=args.fragment_contact_test
    report['npc_rubble_test']=args.npc_rubble_test
    report['npc_projectile_test']=args.npc_projectile_test
    report['firearms_test']=args.firearms_test
    report['fusion_test']=args.fusion_test
    report['player_shield_test']=args.player_shield_test
    report['npc_shield_test']=args.npc_shield_test
    report['npc_rocket_test']=args.npc_rocket_test
    report['npc_grenade_test']=args.npc_grenade_test
    report['expanded_geomod']=args.expanded_geomod
    report['authored_sources']=args.authored_sources
    report['authored_source']=args.authored_source if args.authored_source is not None else (94 if args.dev_room and args.level=='ctf06.rfl' else None)
    (run / 'inputs.bin').write_bytes(payload)
    env = {k: v for k, v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    if args.fragment_contact_test:env['RF_REPLAY_FRAGMENT_CONTACT_TEST']='1'
    if args.fragment_platform_test:env['RF_REPLAY_FRAGMENT_PLATFORM_TEST']='4' if args.ceiling_platform_test else '3' if args.lift_platform_test else '2' if args.tip_platform_test else '1'
    if args.npc_rubble_test:env['RF_REPLAY_DEV_NPC']='2'
    if args.npc_grenade_test:env['RF_REPLAY_DEV_NPC']='3'
    if args.npc_rocket_test:env['RF_REPLAY_DEV_NPC']='4'
    if args.npc_shield_test:env['RF_REPLAY_DEV_NPC']='5'
    if args.firearms_test:env['RF_REPLAY_FIREARMS']=str(args.firearms_test)
    else:env.pop('RF_REPLAY_FIREARMS',None)
    if args.fusion_test:env['RF_REPLAY_FUSION']='1'
    else:env.pop('RF_REPLAY_FUSION',None)
    if args.player_shield_test:env['RF_REPLAY_DEV_NPC']='6'
    env.update(RF_REPLAY_LEVEL=args.level, RF_REPLAY_ARCHIVE=args.archive)
    if args.dev_room:env['RF_REPLAY_DEV_ROOM']='1'
    support_mode='4' if args.tip_support_test else '3' if args.rotate_support_test else '2' if args.release_support_test else '1'
    if args.moving_support_test:env['RF_REPLAY_MOVING_SUPPORT_TEST']=support_mode
    env['RF_REPLAY_AUTHORED_SOURCES']=str(args.authored_sources)
    if args.authored_source is not None:env['RF_REPLAY_AUTHORED_SOURCE']=str(args.authored_source)
    if args.terrain_texture_audit:env['RF_REPLAY_TERRAIN_MATERIAL_AUDIT']=str(run/'pc-terrain-material.bin')
    if args.terrain_atlas_audit:env['RF_REPLAY_TERRAIN_BASE_AUDIT']=str(run/'pc-terrain-atlas.csv')
    if args.terrain_map_limit is not None:
        env.update(RF_REPLAY_TERRAIN_MAP_LIMIT=str(args.terrain_map_limit),RF_REPLAY_TERRAIN_MAP_LIMIT_UNTIL=str(args.terrain_map_limit_until))
        report['terrain_map_fault']=dict(limit=args.terrain_map_limit,until_frame=args.terrain_map_limit_until)
    if args.player_checkpoint:env['RF_REPLAY_PLAYER_CHECKPOINT']='1'
    if args.water_test:env['RF_REPLAY_WATER_TEST']='1'
    if liquid_mode:env['RF_REPLAY_SWIM_TEST']=str(liquid_mode)
    if args.debris_player_test:env['RF_REPLAY_DEBRIS_PLAYER_TEST']='1'
    if args.ripple_test:env['RF_REPLAY_RIPPLE_TEST']='1'
    if args.ripple_test or args.capture_ripple:env['RF_REPLAY_RIPPLE_VERTICES']=str(run/'pc-ripple-vertices.bin')
    if checkpoint:env['RF_REPLAY_GEOMOD_CHECKPOINT_OUT']=str(run/'pc-checkpoint.rfds')
    if args.geomod_checkpoint_in:env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(args.geomod_checkpoint_in.resolve())
    if args.terrain_test_light:env['RF_REPLAY_TERRAIN_TEST_LIGHT']='1'
    if args.cavity_seam_test:env['RF_REPLAY_CAVITY_SEAM_TEST']='1'
    report['cavity_seam_test']=args.cavity_seam_test
    if args.shallow_fixture:env['RF_REPLAY_SHALLOW_FIXTURE']='3' if args.shallow_oblique else '2' if args.shallow_two_limits else '1'
    report['shallow_fixture']=args.shallow_fixture
    report['water_test']=args.water_test
    report['player_checkpoint']=args.player_checkpoint
    report['swim_test']=args.swim_test
    report['liquid_test_mode']=liquid_mode
    report['shallow_two_limits']=args.shallow_two_limits
    report['shallow_oblique']=args.shallow_oblique
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
        subprocess.run(['cmake','--build',pc_build,'--config','Release','--target','rf_pc_play'],
            cwd=root,stdout=out,stderr=subprocess.STDOUT,check=True)
    pc_game=root/'Installed_Game'
    if args.fragment_platform_test:
        subprocess.run([sys.executable,'-B','tools/build_fragment_platform_fixture.py'],cwd=root,check=True)
        pc_game=root/'artifacts/fragment-platform/game'
        report['fragment_platform_mode']='ceiling' if args.ceiling_platform_test else 'lift' if args.lift_platform_test else 'tip' if args.tip_platform_test else 'translate'
        report['fragment_platform_fixture']=json.loads((pc_game.parent/'build.json').read_text())
    pc = subprocess.run([str(root / pc_build / 'Release/rf_pc_play.exe'), '--spawn-replay',
        str(pc_game), str(run / 'inputs.bin'), str(run / 'pc-final.ppm')],
        cwd=root, env=env, capture_output=True, text=True)
    (run / 'pc-reference.txt').write_text(pc.stdout + pc.stderr)
    pc.check_returncode()
    report['pc_sha256'] = hashlib.sha256((root / pc_build / 'Release/rf_pc_play.exe').read_bytes()).hexdigest()
    saved = {p.name: p.read_bytes() for p in disc.glob('campaign-*') if p.is_file()}
    for name in ('player-replay.bin', 'player-control-frames.txt', 'audio-output.flag', 'particle-step-fixtures.bin', 'renderer-cull-off.flag', 'renderer-cull-on.flag', 'renderer-batch-off.flag', 'renderer-world-off.flag', 'renderer-draw-audit.flag'):
        p = disc / name
        saved[name] = p.read_bytes() if p.exists() else None
    for name in ('campaign-spawn.flag', 'campaign-level.bin', 'campaign-actor.bin', 'campaign-setup.bin', 'campaign-item.bin', 'campaign-exit.bin', 'campaign-return.bin', 'campaign-goal.bin', 'campaign-exit-start.bin'):
        saved.setdefault(name, None)
    if args.fragment_platform_test:
        if (disc/'fragment-platform.vpp').exists():
            raise RuntimeError('Unexpected existing disc/fragment-platform.vpp; inspect prior fixture restoration before another run')
        saved['fragment-platform.vpp']=None
        with (disc/'levelsm.vpp').open('rb') as original_archive:
            report['normal_levelsm_sha256']=hashlib.file_digest(original_archive,'sha256').hexdigest()
    process = monitor = None
    saved.setdefault('campaign-trigger-start.bin', None)
    dev_flag=disc/'dev-room.flag'
    saved['dev-room.flag']=dev_flag.read_bytes() if dev_flag.exists() else None
    light_flag=disc/'terrain-test-light.flag'
    saved[light_flag.name]=light_flag.read_bytes() if light_flag.exists() else None
    shallow_flag=disc/'shallow-fixture.flag'
    saved[shallow_flag.name]=shallow_flag.read_bytes() if shallow_flag.exists() else None
    for name in ('fragment-platform-test.flag','fragment-contact-test.flag','cavity-seam.flag','geomod-checkpoint.bin','geomod-checkpoint-out.flag','ripple-test.flag','debris-player-test.flag','water-test.flag','swim-test.flag','player-checkpoint.flag','moving-support-test.flag','dev-npc.flag','fusion-test.flag','firearms-test.flag',
                 'geomod-hdd-load.flag','geomod-hdd-save.flag','geomod-fallback-seed.flag','geomod-fallback-observe.flag',
                 'geomod-fallback0.rfsg','geomod-fallback1.rfsg','terrain-map-limit.bin','authored-source.bin','authored-count.bin'):
        path=disc/name;saved[name]=path.read_bytes() if path.exists() else None
    # Persist restoration bytes before mutating the disc, including absent files.
    (run / 'disc-restore.json').write_text(json.dumps({
        name: data.hex() if data is not None else None for name, data in saved.items()}))
    mapping = ''

    def build():
        with (run / 'build.log').open('ab') as out:
            subprocess.run(['C:/msys64/usr/bin/bash.exe', '--noprofile', '--norc',
                'tools/build-xbox.sh', '--repack'], cwd=root, env=dict(os.environ, MSYSTEM='CLANG64', RF_GEOMOD_EXPANDED_PROFILE='1' if args.expanded_geomod else '0'),
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
        debris_player_flag=disc/'debris-player-test.flag'
        if args.debris_player_test:debris_player_flag.write_bytes(b'')
        elif debris_player_flag.exists():debris_player_flag.unlink()
        ripple_flag=disc/'ripple-test.flag'
        if args.ripple_test:ripple_flag.write_bytes(b'')
        else:ripple_flag.unlink(missing_ok=True)
        (disc / 'campaign-spawn.flag').write_bytes(b'')
        if args.dev_room:(disc/'dev-room.flag').write_bytes(b'')
        if args.fragment_contact_test:(disc/'fragment-contact-test.flag').write_bytes(b'')
        if args.fragment_platform_test:
            (disc/'fragment-platform-test.flag').write_bytes(b'4' if args.ceiling_platform_test else b'3' if args.lift_platform_test else b'2' if args.tip_platform_test else b'1')
            shutil.copyfile(pc_game/'levelsm.vpp',disc/'fragment-platform.vpp')
        if args.firearms_test:(disc/'firearms-test.flag').write_text(str(args.firearms_test))
        else:(disc/'firearms-test.flag').unlink(missing_ok=True)
        if args.fusion_test:(disc/'fusion-test.flag').write_bytes(b'1')
        else:(disc/'fusion-test.flag').unlink(missing_ok=True)
        if args.player_shield_test:(disc/'dev-npc.flag').write_bytes(b'6')
        elif args.npc_shield_test:(disc/'dev-npc.flag').write_bytes(b'5')
        elif args.npc_rocket_test:(disc/'dev-npc.flag').write_bytes(b'4')
        elif args.npc_grenade_test:(disc/'dev-npc.flag').write_bytes(b'3')
        elif args.npc_rubble_test:(disc/'dev-npc.flag').write_bytes(b'2')
        else:(disc/'dev-npc.flag').unlink(missing_ok=True)
        if args.terrain_draw_audit:(disc/'renderer-draw-audit.flag').write_bytes(b'')
        if args.authored_source is not None:(disc/'authored-source.bin').write_bytes(struct.pack('<I',args.authored_source))
        if args.authored_sources>1:(disc/'authored-count.bin').write_bytes(struct.pack('<I',args.authored_sources))
        if args.player_checkpoint:(disc/'player-checkpoint.flag').write_bytes(b'')
        if args.moving_support_test:(disc/'moving-support-test.flag').write_bytes(support_mode.encode('ascii'))
        if args.water_test:(disc/'water-test.flag').write_bytes(b'')
        if liquid_mode:(disc/'swim-test.flag').write_bytes(str(liquid_mode).encode('ascii'))
        if checkpoint:(disc/'geomod-checkpoint-out.flag').write_bytes(b'')
        if args.geomod_checkpoint_in:(disc/'geomod-checkpoint.bin').write_bytes(args.geomod_checkpoint_in.read_bytes())
        if args.cavity_seam_test:(disc/'cavity-seam.flag').write_bytes(b'')
        if args.shallow_fixture:(disc/'shallow-fixture.flag').write_bytes(b'3' if args.shallow_oblique else b'2' if args.shallow_two_limits else b'')
        if args.terrain_test_light:(disc/'terrain-test-light.flag').write_bytes(b'')
        if args.terrain_map_limit is not None:(disc/'terrain-map-limit.bin').write_bytes(struct.pack('<2I',args.terrain_map_limit,args.terrain_map_limit_until))
        (disc / 'campaign-level.bin').write_bytes(('fragment-platform.vpp' if args.fragment_platform_test else args.archive).encode().ljust(64, b'\0') + args.level.encode().ljust(64, b'\0'))
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
                if args.terrain_texture_audit and d[2]==2 and d[37]>0 and 'terrain_texture' not in report:
                    monitor.command('stop')
                    try:
                        report['terrain_texture']=capture_texture(monitor,symbol,run/'pc-terrain-material.bin',run/'xbox-terrain-material.bin')
                        report['terrain_texture']['frame']=d[37]
                    finally:
                        monitor.command('cont')
                if args.terrain_atlas_audit and d[2]==2 and d[37]>0 and 'terrain_atlas' not in report:
                    monitor.command('stop')
                    try:
                        report['terrain_atlas']=capture_atlas(monitor,symbol,run/'pc-terrain-atlas.csv',run)
                        report['terrain_atlas']['frame']=d[37]
                    finally:
                        monitor.command('cont')
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
            fields.append(('rf_scene_fragment_profile',24))
            fields.append(('rf_scene_fragment_stage_ms',8))
            if args.fragment_contact_test:fields.extend((('rf_scene_fragment_contact_audit',64),('rf_scene_fragment_edge_audit',32),('rf_scene_fragment_moving_audit',16),('rf_scene_fragment_support_audit',16)))
            if args.fragment_platform_test:fields.append(('rf_scene_fragment_platform_audit',32))
            if args.npc_rubble_test:fields.append(('rf_scene_dev_npc_cover',48))
            if args.moving_support_test:fields.append(('rf_scene_moving_support_test',160))
            if args.rotate_support_test:fields.append(('rf_scene_rotating_support_test',120))
            snap = dict(symbols={name: dict(words=words(monitor, symbol(name), count)) for name, count in fields})
            (run / 'guest-memory-final.json').write_text(json.dumps(snap, indent=2))
            edits=snap['symbols']['rf_scene_terrain_edit_times']['words']
            (run/'terrain-edit-times.json').write_text(json.dumps([dict(zip(('frame','cut_ms','bind_ms','debris_prepare_ms','debris_spawn_ms'),edits[i:i+5])) for i in range(0,40,5) if edits[i]],indent=2)+'\n')
            report['checks'] = {}
            fragment_pc=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_PROFILE ') for v in line.split()[1:]]
            fragment_xbox=snap['symbols']['rf_scene_fragment_profile']['words']
            work_indices=list(range(10))+[15,19,20,21,22]
            assert len(fragment_pc)==24 and fragment_xbox[0]==2
            assert all(fragment_pc[i]==fragment_xbox[i] for i in work_indices), 'Fragment collision work differs on Xbox'
            report['checks']['FRAGMENT_PROFILE_WORK']=dict(pc=fragment_pc,xbox=fragment_xbox,compared_indices=work_indices,equal=True)
            profile_names=('version','ticks','active_ticks','active_bodies_total','max_active_bodies','queries','corner_casts','reciprocal_vertices','triangle_tests','pose_evaluations','total_ms','active_ms','max_tick_ms','max_tick_frame','max_active_tick_ms','max_queries_per_tick','max_active_tick_frame','bodies_at_peak','queries_at_peak','triangle_preparations','local_rejections','shape_preparations','cache_fallbacks','reserved')
            report['fragment_profile']=dict(zip(profile_names,fragment_xbox))
            stage_words=snap['symbols']['rf_scene_fragment_stage_ms']['words']
            report['fragment_stage_ms']={name:dict(total=stage_words[i*2],max_query=stage_words[i*2+1]) for i,name in enumerate(('spheres','corners','mover_vertices','world_vertices'))}
            report['fragment_profile']['mean_active_tick_ms']=fragment_xbox[11]/fragment_xbox[2] if fragment_xbox[2] else None

            if args.fragment_platform_test:
                expected=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_PLATFORM ') for v in line.split()[1:]]
                actual=snap['symbols']['rf_scene_fragment_platform_audit']['words']
                assert len(expected)==32 and expected==actual and actual[:2]==[599,60], 'Platform sequence differs or incomplete'
                assert actual[2]==1, 'Repeated sleep/wake while platform moves'
                if args.tip_platform_test or args.lift_platform_test:assert actual[25:27]==[61,0], 'Platform motion exhausted collision substeps'
                basis=struct.unpack('<9f',struct.pack('<9I',*actual[16:25]))
                assert basis==((0,-1,0,1,0,0,0,0,1) if args.tip_platform_test else (1,0,0,0,1,0,0,0,1))
                initial,final=struct.unpack('<2f',struct.pack('<2I',*actual[6:8]))
                assert abs(initial-.65)<.005 and 420<actual[8]<=480
                if args.lift_platform_test:
                    support_y,gap=struct.unpack('<2f',struct.pack('<2I',*actual[28:30]))
                    assert actual[27]==1 and abs(support_y-1.15)<.005 and abs(gap)<=.01
                elif args.ceiling_platform_test:
                    peak=struct.unpack('<f',struct.pack('<I',actual[30]))[0]
                    assert 0<peak<4, 'Ceiling contact added excessive kinetic energy'
                    report['ceiling_response']={'peak_speed':peak**.5,'limited_frames':actual[31],
                        'scope':'Energy regression only; blocked/crush resolution remains incomplete'}
                else:assert abs(final+1.5)<.005
                xyz=struct.unpack('<3f',struct.pack('<3I',*actual[3:6]))
                assert abs(xyz[0]-(9.449 if args.tip_platform_test or args.lift_platform_test or args.ceiling_platform_test else 12.449))<.00001
                assert abs(xyz[1]-(2.05 if args.ceiling_platform_test else 1.05 if args.lift_platform_test else .55))<.00001 and xyz[2]==2.5
                assert actual[9]==1 and actual[14]==1 and not actual[13]&0x80000000
                report['checks']['FRAGMENT_PLATFORM']=dict(pc=expected,xbox=actual,equal=True,initial_bottom=initial,final_bottom=final,drop=initial-final)

            if args.fragment_contact_test:
                expected=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_CONTACT_AUDIT ') for v in line.split()[1:]]
                actual=snap['symbols']['rf_scene_fragment_contact_audit']['words']
                assert len(expected)==64 and expected==actual, 'Fragment contact fixture differs on Xbox'
                assert actual[:4]==[2,8,0,1] and actual[4:7]==[0,8,4], 'Incomplete contact fixture or missing baseline misses'
                for row,fraction,identity,material in ((1,.25,17,3),(2,.25,77,7),(3,.25,77,7),(4,.125,77,7)):
                    words_=actual[4+row*8:12+row*8]
                    assert words_[:2]==[row,1] and struct.unpack('<f',struct.pack('<I',words_[2]))[0]==fraction
                    normal=struct.unpack('<3f',struct.pack('<3I',*words_[3:6]))
                    assert normal==((0.,1.,0.) if row<3 else (-1.,0.,0.)) and words_[6:]==[identity,material]
                assert actual[44:46]==[5,0] and actual[50]==77
                assert actual[52]==6 and actual[53]!=0 and actual[54:56]==[1,0]
                assert actual[60:62]==[7,1] and struct.unpack('<2f',struct.pack('<2I',*actual[62:64]))==(.25,1.)
                report['checks']['FRAGMENT_CONTACT_AUDIT']=dict(pc=expected,xbox=actual,equal=True,cases=8)
                edge_pc=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_EDGE_AUDIT ') for v in line.split()[1:]]
                edge_xbox=snap['symbols']['rf_scene_fragment_edge_audit']['words']
                assert len(edge_pc)==32 and edge_pc==edge_xbox and edge_xbox[:4]==[1,4,0,1]
                for row,identity,material,normal in ((0,17,3,(0.,1.,0.)),(1,77,7,(0.,1.,0.)),(2,77,7,(-1.,0.,0.))):
                    w=edge_xbox[4+row*7:11+row*7]
                    assert w[0]==1 and struct.unpack('<4f',struct.pack('<4I',*w[1:5]))==(.25,*normal) and w[5:]==[identity,material]
                assert edge_xbox[25]!=0 and edge_xbox[26:28]==[1,0]
                report['checks']['FRAGMENT_EDGE_AUDIT']=dict(pc=edge_pc,xbox=edge_xbox,equal=True,cases=4)
                moving_pc=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_MOVING_AUDIT ') for v in line.split()[1:]]
                moving_xbox=snap['symbols']['rf_scene_fragment_moving_audit']['words']
                assert moving_pc==moving_xbox and moving_pc[:4]==[1,3,0,1]
                assert moving_pc[4:]==[1,0x3ec00000,0x3f400000,0x3f800000]*3
                report['checks']['FRAGMENT_MOVING_AUDIT']=dict(pc=moving_pc,xbox=moving_xbox,equal=True,cases=3)
                support_pc=[int(v) for line in pc.stdout.splitlines() if line.startswith('FRAGMENT_SUPPORT_AUDIT ') for v in line.split()[1:]]
                support_xbox=snap['symbols']['rf_scene_fragment_support_audit']['words']
                assert support_pc==support_xbox and support_pc[:8]==[1,5,0,1,1,0,0xffffffff,1]
                before,after,velocity=struct.unpack('<3f',struct.pack('<3I',*support_pc[8:11]))
                assert before==.25 and after<before and velocity<0 and support_pc[11]&0x80000000
                assert support_pc[12:]==[0,1,1,2]
                report['checks']['FRAGMENT_SUPPORT_AUDIT']=dict(pc=support_pc,xbox=support_xbox,equal=True,cases=5)



            if args.terrain_texture_audit:
                assert 'terrain_texture' in report, 'No live frame available for texture audit'
                report['checks']['TERRAIN_TEXTURE']=report['terrain_texture']
            if args.terrain_atlas_audit:
                assert 'terrain_atlas' in report, 'No live frame available for atlas audit'
                report['checks']['GPU_TERRAIN_ATLAS']=report['terrain_atlas']
            if args.terrain_draw_audit:
                report['terrain_draws']=capture_draws(monitor,symbol,run/'pc-terrain-material.bin',report['terrain_atlas'],run)
                report['checks']['GPU_TERRAIN_DRAWS']=report['terrain_draws']
            if checkpoint:
                state=words(monitor,symbol('rf_scene_geomod_checkpoint_state'),4)
                memory=words(monitor,symbol('rf_scene_geomod_checkpoint_memory'),2)
                pointer=words(monitor,symbol('rf_scene_geomod_checkpoint_data'),1)[0]
                assert state[0]==0 and state[3]==1 and 288<=state[1]<=checkpoint_limit and pointer,'Checkpoint export failed'
                data=bytearray()
                for offset in range(0,state[1],4096):
                    count=(min(4096,state[1]-offset)+3)//4
                    data.extend(struct.pack('<'+'I'*count,*words(monitor,pointer+offset,count)))
                data=bytes(data[:state[1]])
                (run/'xbox-checkpoint.rfds').write_bytes(data)
                expected=(run/'pc-checkpoint.rfds').read_bytes()
                value=2166136261
                for byte in data:value=((value^byte)*16777619)&0xffffffff
                report['checks']['GEOMOD_CHECKPOINT']=dict(equal=data==expected,state=state,memory=memory,
                    sha256=hashlib.sha256(data).hexdigest(),pc_sha256=hashlib.sha256(expected).hexdigest())
                assert value==state[2],'Checkpoint readback hash mismatch'
                assert 0<memory[0]<=memory[1]<=checkpoint_limit,'Checkpoint external memory budget'
                assert data==expected,'PC/Xbox destruction checkpoint differs'
            if args.ripple_test or args.capture_ripple:
                state=words(monitor,symbol('rf_scene_ripple_vertex_state'),5)
                assert state[2]<=384 and state[3]==56 and state[4]==0,'Ripple vertex capture overflow'
                data=struct.pack('<5I',*state)
                count=state[2]*14
                if count:data+=struct.pack('<'+'I'*count,*words(monitor,symbol('rf_scene_ripple_vertices'),count))
                for name,count in [('rf_scene_ripple_camera',12),('rf_scene_ripple_sources',96),('rf_scene_ripple_input_count',1),('rf_scene_ripple_input',240),('rf_scene_ripple_fp_state',4),('rf_scene_ripple_local',240)]:
                    data+=struct.pack('<'+'I'*count,*words(monitor,symbol(name),count))
                (run/'xbox-ripple-vertices.bin').write_bytes(data)
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
            if args.npc_rubble_test:
                expected=[int(value) for line in pc.stdout.splitlines() if line.startswith('DEV_NPC_COVER ') for value in line.split()[1:]]
                actual=snap['symbols']['rf_scene_dev_npc_cover']['words']
                assert len(expected)==48 and expected==actual, 'NPC cover sequence missing or differs on Xbox'
                for i in range(6):
                    row=actual[i*8:i*8+8]
                    assert row[0]==660+i*60
                    expected_counts=[i+1,0,i+1] if i<3 else ([4,1,3] if args.npc_projectile_test else [i+1,i-2,3])
                    assert row[1:4]==expected_counts and row[5]==int(i<3)
                    health=struct.unpack('<f',struct.pack('<I',row[4]))[0]
                    assert health==100 if i<3 else health<100
                report['checks']['DEV_NPC_COVER']=dict(pc=expected,xbox=actual,equal=True)
            if args.moving_support_test:
                expected=[int(value) for line in pc.stdout.splitlines() if line.startswith('MOVING_SUPPORT ') for value in line.split()[1:]]
                actual=snap['symbols']['rf_scene_moving_support_test']['words']
                assert len(expected)==160, 'Incomplete PC moving-support sequence'
                report['checks']['MOVING_SUPPORT']=dict(pc=expected,xbox=actual,equal=expected==actual)
                assert expected==actual, 'Moving-support sequence differs on Xbox'
            if args.rotate_support_test:
                expected=[int(value) for line in pc.stdout.splitlines() if line.startswith('ROTATING_SUPPORT ') for value in line.split()[1:]]
                actual=snap['symbols']['rf_scene_rotating_support_test']['words']
                assert len(expected)==120, 'Incomplete PC rotation sequence'
                report['checks']['ROTATING_SUPPORT']=dict(pc=expected,xbox=actual,equal=expected==actual)
                assert expected==actual, 'Rotating-support sequence differs on Xbox'
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
            for name, label, count in [('rf_scene_authored_identity', 'AUTHORED_IDENTITY', 10), ('rf_scene_terrain_publication', 'TERRAIN_PUBLICATION', 8), ('rf_scene_debris_audio', 'DEBRIS_AUDIO', 14), ('rf_scene_player_checkpoint_state', 'PLAYER_CHECKPOINT', 8), ('rf_scene_liquid_damage', 'LIQUID_DAMAGE', 8), ('rf_scene_player_swim', 'PLAYER_SWIM', 12), ('scene_actor_body', 'PC_PLAY_BODY', 77),
                    ('rf_scene_player_ammo', 'PLAYER_AMMO', 8), ('rf_scene_combat', 'COMBAT', 8),
                    ('rf_scene_script_movement', 'SCRIPT_MOVE', 8), ('rf_scene_enemy_combat', 'ENEMY_COMBAT', 8),
                    ('rf_scene_rotating_doors', 'ROTATING_DOORS', 8), ('rf_scene_script_attack', 'SCRIPT_ATTACK', 12), ('rf_scene_attack_recovery', 'ATTACK_RECOVERY', 4), ('rf_scene_enemy_damage_kinds', 'ENEMY_DAMAGE_KINDS', 10), ('rf_scene_enemy_melee', 'ENEMY_MELEE', 4), ('rf_scene_enemy_spread', 'ENEMY_SPREAD', 8), ('rf_scene_combat_pain', 'COMBAT_PAIN', 8), ('rf_scene_pain_attack_gate', 'PAIN_ATTACK_GATE', 6), ('rf_scene_weapon_drops', 'WEAPON_DROPS', 8), ('rf_scene_rifle_alt', 'RIFLE_ALT', 8), ('rf_scene_shotgun', 'SHOTGUN', 8), ('rf_scene_grenades', 'GRENADES', 8), ('rf_scene_ai_grenades', 'AI_GRENADES', 5), ('rf_scene_ai_rockets', 'AI_ROCKETS', 5), ('rf_scene_riot_shield', 'RIOT_SHIELD', 4), ('rf_scene_player_shield', 'PLAYER_SHIELD', 4), ('rf_scene_fusion_projectiles', 'FUSION_PROJECTILES', 5), ('rf_scene_machine_mode', 'MACHINE_MODE', 8), ('rf_scene_remote', 'REMOTE', 8), ('rf_scene_flame_visual', 'FLAME_VISUAL', 6), ('rf_scene_flame_canister', 'FLAME_CANISTER', 5), ('rf_scene_burning', 'BURNING', 5), ('rf_scene_burning_visual', 'BURNING_VISUAL', 5), ('rf_scene_rockets', 'ROCKETS', 8), ('rf_scene_rocket_blast', 'ROCKET_BLAST', 8), ('rf_scene_rocket_visual', 'ROCKET_VISUAL', 8), ('rf_scene_ripple_visual', 'RIPPLE_VISUAL', 8), ('rf_scene_ripple_lifecycle', 'RIPPLE_LIFECYCLE', 4), ('rf_scene_rocket_liquid', 'ROCKET_LIQUID_STATE', 4), ('rf_scene_enemy_fire', 'ENEMY_FIRE', 6),
                    ('rf_scene_use_reach', 'USE_REACH', 4), ('rf_scene_debris_wet','DEBRIS_WET_STATE',8), ('rf_scene_debris_visibility','DEBRIS_VISIBILITY',8), ('rf_scene_debris_motion','DEBRIS_MOTION',8), ('rf_scene_debris_crossing','DEBRIS_CROSSING',8), ('rf_scene_debris_splash_audio','DEBRIS_SPLASH_AUDIO',9), ('rf_scene_debris_player','DEBRIS_PLAYER',8), ('rf_scene_debris_rotation','DEBRIS_ROTATION',4), ('rf_scene_debris_cleanup','DEBRIS_CLEANUP',8), ('rf_scene_debris_blood','DEBRIS_BLOOD',8), ('rf_scene_debris_player_test','DEBRIS_PLAYER_TEST',8),
                    ('rf_scene_particles_summary', 'SCENE_PARTICLES', 8), ('rf_scene_live_motion', 'LIVE_MOTION', 8), ('rf_scene_airlock', 'AIRLOCK', 6), ('rf_scene_script_animation', 'SCRIPT_ANIMATION', 10), ('rf_scene_alarm', 'ALARM', 12), ('rf_scene_switch_runtime', 'SWITCH_RUNTIME', 8), ('rf_scene_switch_detail', 'SWITCH_DETAIL', 8), ('rf_scene_switch_history', 'SWITCH_HISTORY', 4), ('rf_scene_trigger_history', 'TRIGGER_HISTORY', 4), ('rf_scene_startup_inventory', 'STARTUP_INVENTORY', 4), ('rf_scene_pickups', 'PICKUPS', 8), ('rf_scene_pickup_vitals', 'PICKUP_VITALS', 4), ('rf_scene_riot', 'RIOT_STICK', 8), ('rf_scene_weapon_selection', 'WEAPON_SELECTION', 8),
                    ('rf_scene_player_weapon', 'PLAYER_WEAPON', 8), ('rf_scene_weapon_audio', 'WEAPON_AUDIO', 9), ('rf_scene_impact_audio', 'IMPACT_AUDIO', 9),
                    ('rf_scene_rocket_contacts', 'ROCKET_CONTACTS', 8),
                    ('rf_scene_combat_death', 'COMBAT_DEATH', 8)]:
                expected = list(map(int, next(line for line in pc.stdout.splitlines() if line.startswith(label + ' ')).split()[1:]))
                actual = words(monitor, symbol(name), count)
                # PICKUPS[6] counts CPU-emitted vertices. Xbox retained GPU
                # submission bypasses those vertices in scene_weapon_submit.
                indices=[i for i in range(count) if label!='PICKUPS' or i!=6]
                equal=all(actual[i]==expected[i] for i in indices)
                report['checks'][label] = dict(equal=equal, all_words_equal=actual==expected,
                    compared_indices=indices, xbox=actual, pc=expected)
                if label=='AUTHORED_IDENTITY' and args.dev_room and args.level=='ctf06.rfl':
                    assert actual[9]==1 and any(actual[:8]) and 0<actual[8]<=2*1024*1024,'Authored identity capture missing/overbudget'
                    report['authored_source_identity']=dict(sha256=struct.pack('<8I',*actual[:8]).hex(),
                        additional_capture_peak_bytes=actual[8],scope='Immutable authored source/material/chart identity, not saved destruction state')
                if label=='PICKUPS':
                    report['pickup_cpu_vertices']=dict(xbox=actual[6],pc=expected[6],
                        scope='Backend-specific rendering count; excluded from gameplay parity')
                assert equal, label
            expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_PIECES ')).split()[1:]))
            actual=words(monitor,symbol('rf_scene_detached_pieces'),6)
            indices=[0,1,2,3,5] # Resident owner size depends on pointer width.
            equal=all(actual[i]==expected[i] for i in indices)
            budget_ok=all(0<=v[4]<=2*1024*1024 for v in (actual,expected))
            report['checks']['DETACHED_PIECES']=dict(equal=equal,budget_ok=budget_ok,
                compared_indices=indices,xbox=actual,pc=expected,
                scope='Ownership and posed draw counts, not motion or pixel fidelity')
            assert equal and budget_ok and actual[5]==0,'Detached ownership/draw mismatch'
            expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_MOTION ')).split()[1:]))
            actual=words(monitor,symbol('rf_scene_detached_motion'),8)
            report['checks']['DETACHED_MOTION']=dict(equal=actual==expected,xbox=actual,pc=expected)
            assert actual==expected and actual[6]==0,'Detached motion mismatch'
            expected_pose=list(map(float,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_POSE ')).split()[1:]))
            pose_bits=list(struct.unpack('<6I',struct.pack('<6f',*expected_pose)))
            actual_pose=words(monitor,symbol('rf_scene_detached_pose'),6)
            report['checks']['DETACHED_POSE']=dict(equal=actual_pose==pose_bits,xbox=actual_pose,pc=pose_bits)
            assert actual_pose==pose_bits,'Detached pose mismatch'
            if args.npc_projectile_test:
                contacts=report['checks']['ROCKET_CONTACTS']['xbox']
                assert contacts[1:4]==[1,0,1] and contacts[5:7]==[0x70000001,0], 'No direct rocket actor hit'
                assert contacts[4]==struct.unpack('<I',struct.pack('<f',400))[0]
            expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_ROCKET ')).split()[1:]))
            actual=words(monitor,symbol('rf_scene_detached_rocket'),7)
            report['checks']['DETACHED_ROCKET']=dict(equal=actual==expected,xbox=actual,pc=expected)
            assert actual==expected and actual[6]==0,'Detached rocket contact mismatch'
            expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_HITSCAN ')).split()[1:]))
            actual=words(monitor,symbol('rf_scene_detached_hitscan'),7)
            report['checks']['DETACHED_HITSCAN']=dict(equal=actual==expected,xbox=actual,pc=expected)
            assert actual==expected and actual[6]==0,'Detached hitscan contact mismatch'
            expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DETACHED_PLAYER ')).split()[1:]))
            actual=words(monitor,symbol('rf_scene_detached_player'),7)
            report['checks']['DETACHED_PLAYER']=dict(equal=actual==expected,xbox=actual,pc=expected)
            assert actual==expected and actual[6]==0,'Detached player contact mismatch'
            if args.debris_player_test:
                from verify_debris_player_scenario import verify
                report['debris_player_scenario']=verify(report)
            if args.water_test and args.frames == 180:
                report['water_scenario'] = verify_water_scenario(report)
            if args.dev_room:
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('DEBRIS_RELAUNCH_STATE ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_debris_relaunch'),8)
                report['checks']['DEBRIS_RELAUNCH_STATE']=dict(equal=actual==expected,xbox=actual,pc=expected)
                assert actual==expected,'Debris relaunch mismatch'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('GEOMOD ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_geomod'),8)
                # Allocation sizes differ by pointer width. Compare terrain
                # presence/history/generation/status, and enforce both budgets.
                indices=[0,1,2,5,6,7]
                equal=all(actual[i]==expected[i] for i in indices)
                # Authored solid edits reserve old+clone core, two publication
                # banks, piece registries and private lighting staging. Default13MiB;
                # connected pairs reserve16MiB and the triple profile17MiB.
                terrain_budget=(16*1024*1024 if args.level=='ctf06.rfl' else 2359296+65536) if args.expanded_geomod else (13*1024*1024 if args.level=='ctf06.rfl' and args.dev_room else 1024*1024+65536)
                if args.level=='ctf06.rfl' and args.authored_source in (92,95,98,108) and args.authored_sources in (2,3):
                    terrain_budget=(17 if args.authored_sources==3 else 16)*1024*1024
                budget_ok=all(0<=v[3]<=v[4]<=terrain_budget for v in (actual,expected))
                report['checks']['GEOMOD']=dict(equal=equal,budget_ok=budget_ok,budget_bytes=terrain_budget,
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
                budget_ok=actual[3]<=(1536*1024 if args.expanded_geomod else 1280*1024)
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
                assert equal and actual[1]<=(18432 if args.expanded_geomod else 8192) and actual[3]<=(1024*1024 if args.expanded_geomod else 320*1024),'TERRAIN_DRAW'
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('TERRAIN_NOISE ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_terrain_noise'),8)
                equal=actual==expected
                report['checks']['TERRAIN_NOISE']=dict(equal=equal,xbox=actual,pc=expected,
                    scope='Persistent generated-face mappings, retained texel checks, bounded owner and generation')
                # No generated faces means this lazy owner has never been allocated.
                assert equal and (actual==[0]*8 or (actual[0]==1 and actual[1]<=(2048 if args.expanded_geomod else 1024) and actual[6]<=(256*1024 if args.expanded_geomod else 128*1024))),'TERRAIN_NOISE'
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



            if args.dev_room and args.level=='ctf06.rfl':
                expected=list(map(int,next(line for line in pc.stdout.splitlines() if line.startswith('AUTHORED_COLLECTION ')).split()[1:]))
                actual=words(monitor,symbol('rf_scene_authored_collection'),6)
                equal=actual[:5]==expected[:5]
                report['checks']['AUTHORED_COLLECTION']=dict(equal=equal,xbox=actual,pc=expected,compared_indices=list(range(5)))
                assert equal and actual[0]==args.authored_sources, 'Authored collection ownership mismatch'
                cut_rows=[line for line in pc.stdout.splitlines() if line.startswith('AUTHORED_SOURCE_CUTS ')]
                if cut_rows:
                    expected=list(map(int,cut_rows[-1].split()[1:]));actual=words(monitor,symbol('rf_scene_authored_source_cuts'),8)
                    report['checks']['AUTHORED_SOURCE_CUTS']=dict(equal=actual==expected,xbox=actual,pc=expected)
                    assert actual==expected,'Per-source cut history mismatch'


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
            # Restore the disc image without compiling source that may have
            # changed while the emulator was running. Preserve the tested XBE.
            for name, data in saved.items():
                actual = (disc / name).read_bytes() if (disc / name).exists() else None
                if actual != data:
                    raise RuntimeError('Disc restoration mismatch: ' + name)
            if args.fragment_platform_test:
                with (disc/'levelsm.vpp').open('rb') as original_archive:
                    assert hashlib.file_digest(original_archive,'sha256').hexdigest()==report['normal_levelsm_sha256'], 'Normal archive changed during fixture'
            iso = root / 'build/xbox/redfaction-diagnostic.iso'
            temporary = run / 'restored-disc.iso'
            xbe = disc / 'default.xbe'
            before = hashlib.sha256(xbe.read_bytes()).hexdigest()
            with (run / 'restore-pack.log').open('wb') as out:
                subprocess.run(['C:/nxdk/tools/extract-xiso/build/extract-xiso.exe',
                    '-c', str(disc), str(temporary)], cwd=root,
                    stdout=out, stderr=subprocess.STDOUT, check=True)
            if hashlib.sha256(xbe.read_bytes()).hexdigest() != before:
                raise RuntimeError('XBE changed during disc restoration')
            os.replace(temporary, iso)
            report['disc_restored'] = True
            report['restore_xbe_sha256'] = before
        except Exception as exc:
            report.update(result='FAIL', restore_error=repr(exc))
            raise
        finally:
            (run / 'report.json').write_text(json.dumps(report, indent=2))
            print(run, report['result'], flush=True)


if __name__ == '__main__':
    main()
