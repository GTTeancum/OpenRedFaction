"""Bounded Xbox save during authored miner death, then resume to its frozen end.

Source32/load120 neutral frames; Slay9362 -> miner8432. Read-only guest probes
inspect the actual owned pose/bone generations. No host input, images, PC
runtime, campaign route or compiled fixture injection. Parent runs this tool.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import struct

from build_fragment_platform_fixture import read_entry
from xemu_turret_combat import entity_rows
from xemu_corpse_save import SYMBOLS as SETTLED_SYMBOLS
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR, SLAY = 8432, 9362
SAVE_FRAMES, LOAD_FRAMES = 32, 120
SYMBOLS = dict(SETTLED_SYMBOLS, rf_scene_corpse_unsettled_checkpoint=4,
               rf_scene_weapon_drops=8)


def actor_slot():
    rows = entity_rows(read_entry(ROOT/'Installed_Game/levels1.vpp', 'L1S1.rfl'))
    matches = [(i, row) for i, row in enumerate(rows) if row['uid'] == ACTOR]
    if len(matches) != 1 or matches[0][1]['name'].lower() != 'miner1':
        raise RuntimeError('Installed L1S1 miner8432 identity changed')
    return matches[0][0]


def pose_probe(slot):
    def probe(monitor, mapping):
        count = words(monitor, address(mapping, 'campaign_model_owner_count'), 1)[0]
        owners = words(monitor, address(mapping, 'campaign_model_owners'), 1)[0]
        if slot >= count or not owners:
            raise RuntimeError('Miner model owner is missing')
        # Exact32-bit C layout: campaign_model_owner in scene.c is80 bytes:
        # registration16,pose4,position12,basis36,appearance4,room4,owned4.
        owner = words(monitor, owners+80*slot, 20)
        pose_address = owner[4]
        if not owner[0] or not owner[19] or not pose_address or owner[1] != pose_address+8:
            raise RuntimeError('Miner pose is not published through its owned model')
        # rf_entity_pose: skeleton/bones8 + playback260 + controller24 +
        # matrices/generations/overrides pointers12 =304 bytes (x86 NXDK ABI).
        pose = words(monitor, pose_address, 76)
        bones, generation = pose[1], pose[65]
        if not 0 < bones <= 50 or generation > 65535 or not pose[73] or not pose[74]:
            raise RuntimeError('Invalid owned skeletal pose layout')
        stamps_raw = struct.pack('<'+'I'*((bones+1)//2),
                                *words(monitor, pose[74], (bones+1)//2))
        stamps = list(struct.unpack_from('<'+'H'*bones, stamps_raw))
        matrices = words(monitor, pose[73], bones*12)
        return dict(slot=slot, bones=bones, active_clips=pose[2], motion=pose[3],
                    tick=pose[4], frozen=pose[54], generation=generation,
                    freeze_slot=pose[51], owned=True, current_bones=sum(s == generation for s in stamps),
                    matrix_sha256=hashlib.sha256(struct.pack('<'+'I'*len(matrices), *matrices)).hexdigest())
    return probe


def saved_death(payload):
    if len(payload) < 320 or payload[:4] != b'RFWC':
        raise RuntimeError('Missing ordinary world checkpoint')
    kind, offset, size = struct.unpack_from('<III', payload, 128+12)
    if kind != 2 or offset < 320 or offset+size > len(payload):
        raise RuntimeError('Invalid NPC component bounds')
    blob = payload[offset:offset+size]
    if len(blob) < 64 or blob[:4] != b'RFNC' or struct.unpack_from('<I', blob, 4)[0] != 10:
        raise RuntimeError('Expected RFNC10 with unchanged row layout')
    count = struct.unpack_from('<I', blob, 16)[0]
    at, result = 64, None
    for _ in range(count):
        if at+600 > len(blob):
            raise RuntimeError('Truncated NPC row')
        uid = struct.unpack_from('<I', blob, at)[0]
        animation = struct.unpack_from('<I', blob, at+540)[0]
        move, combat = struct.unpack_from('<II', blob, at+564)
        shots = struct.unpack_from('<I', blob, at+588)[0]
        end = at+600+move+combat+24*shots+animation
        if end > len(blob):
            raise RuntimeError('NPC extension exceeds component')
        if uid == ACTOR:
            if result is not None or animation != 120:
                raise RuntimeError('Expected exactly one saved miner/single death clip')
            dead, flags, action = struct.unpack_from('<IIi', blob, at+552)
            p = at+600+move+combat+24*shots
            active, freeze = struct.unpack_from('<Ii', blob, p+16)
            frozen = struct.unpack_from('<I', blob, p+32)[0]
            generation = struct.unpack_from('<I', blob, p+76)[0]
            motion, tick, weight = struct.unpack_from('<iif', blob, p+108)
            result = dict(uid=uid, dead=dead, flags=flags, action=action,
                          active_clips=active, freeze_slot=freeze, frozen=frozen,
                          motion=motion, tick=tick, weight=weight, generation=generation)
        at = end
    if at != len(blob) or result is None:
        raise RuntimeError('Malformed NPC component or missing miner')
    if result['dead'] != 1 or not result['flags'] & 1 or not 5 <= result['action'] <= 16 or \
       result['active_clips'] != 1 or result['frozen'] != 0 or result['freeze_slot'] != 0 or result['weight'] <= 0:
        raise RuntimeError(f'Saved death is not an in-progress single end-freezing clip: {result}')
    return result


def check_run(result, frames):
    if result['guest_phase'] != 5 or result['frames'] != frames or \
       result['memory_bytes'] != 64*1024*1024 or result['free_pages'] <= 0 or result['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB run or dead player')
    corpse = result['extra']['rf_scene_live_corpses']
    if corpse[0] != 1 or corpse[1] != 1 or not corpse[4] or any(corpse[5:]):
        raise RuntimeError(f'Corpse owner/update failure: {corpse}')
    pose = result['probe']
    if not pose['owned'] or pose['active_clips'] != 1 or pose['current_bones'] != pose['bones']:
        raise RuntimeError(f'Corpse pose generation did not reach its actual bone matrices: {pose}')


def validate_source(saved, payload):
    check_run(saved, SAVE_FRAMES)
    x, state = saved['extra'], saved['checkpoint_state']
    if x['rf_scene_setup_result'] != [1, SLAY, 1, 0] or state[9] != 1 or state[3] or state[4] != len(payload):
        raise RuntimeError(f'Authored Slay or ordinary save failed: {x["rf_scene_setup_result"]}, {state}')
    row = saved_death(payload)
    admitted = x['rf_scene_corpse_unsettled_checkpoint']
    if not admitted[0] or admitted[1:3] != [ACTOR, row['tick']] or admitted[3]:
        raise RuntimeError(f'Unsettled admission did not accept the saved source clock: {admitted}')
    for key in ('motion', 'tick', 'generation', 'frozen'):
        if saved['probe'][key] != row[key]:
            raise RuntimeError(f'Source live pose differs from saved {key}')
    return row


def validate(saved, loaded, payload):
    row = validate_source(saved, payload);check_run(loaded, LOAD_FRAMES)
    x, state, pose = loaded['extra'], loaded['checkpoint_state'], loaded['probe']
    restored = x['rf_scene_corpse_checkpoint']
    if state[8] != 1 or state[0] or state[1] != len(payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError(f'Ordinary load failed: {state}, {x["rf_scene_world_load_reject"]}')
    if restored != [1, 1, 1, 0, ACTOR, 0, 1, 0]:
        raise RuntimeError(f'Restored corpse did not preserve complete unfrozen playback: {restored}')
    if any(x['rf_scene_live_death_audio']) or any(x['rf_scene_script_slays']) or x['rf_scene_weapon_drops'][0]:
        raise RuntimeError('Load replayed death audio, Slay or weapon-drop creation')
    if pose['frozen'] != 1 or pose['motion'] != row['motion'] or pose['tick'] <= row['tick']:
        raise RuntimeError(f'Restored animation did not continue to its frozen end: {row}, {pose}')
    advanced = (pose['generation']-row['generation']) & 65535
    if not advanced or pose['matrix_sha256'] == saved['probe']['matrix_sha256']:
        raise RuntimeError('Death clock or actual bone matrices did not progress after load')
    return dict(result='PASS', saved_death=row, restored_transfer=restored,
                final_pose=pose, generation_updates=advanced,
                free_pages=min(saved['free_pages'], loaded['free_pages']),
                limitations='One authored single-clip death; timed tails/blends/corpse age and visual parity remain outside this check.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing', type=Path, help='Read existing evidence only; no build/emulator')
    args = parser.parse_args()
    if args.validate_existing:
        folder = args.validate_existing
        if not json.loads((folder/'report.json').read_text()).get('disc_restored'):
            raise RuntimeError('Disc restoration not confirmed')
        print(json.dumps(validate(json.loads((folder/'save/result.json').read_text()),
              json.loads((folder/'load/result.json').read_text()), (folder/'save/xbox-world.rfwc').read_bytes()), indent=2))
        return
    require_no_project_xemu(ROOT);slot = actor_slot()
    hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('corpse-unsettled-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()} | {'player-control.flag'}
    original = {n: (DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__, actor_slot=slot, phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64,b'\0')+b'L1S1.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<I',SLAY))
        (DISC/'campaign-actor.bin').write_bytes(struct.pack('<I',ACTOR))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        # tech01_death_leg_R spans160..6560 (80 frames);32 is past transfer,
        # before completion.120 after reload safely exceeds the remaining span.
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved = run_guest(folder,'save',hdd,SAVE_FRAMES,360,capture_world=True,
                          extra_symbols=SYMBOLS,probe=pose_probe(slot),probe_frame=SAVE_FRAMES-1,
                          allow_guest_error=True)
        report['phases']['save'] = saved
        payload = (folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_death'] = validate_source(saved,payload)
        (DISC/'campaign-setup.bin').unlink();(DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(LOAD_FRAMES*48))
        build(folder,'load')
        loaded = run_guest(folder,'load',hdd,LOAD_FRAMES,360,snapshot=True,
                           extra_symbols=SYMBOLS,probe=pose_probe(slot),probe_frame=80)
        report['phases']['load'] = loaded;report.update(validate(saved,loaded,payload))
    except Exception as exc:
        report['error'] = str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None) == data for n,data in original.items())
            if not report['disc_restored']:report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__ == '__main__':main()
