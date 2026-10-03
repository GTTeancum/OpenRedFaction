"""One stock64MiB ordinary save/load of the oldest fading owned miner body.

Uses the six-miner lifecycle fixture with ordinary Slay delayed1s to allow
player grounding before ordinary save admission.
Saves at frame92 (corpse age about32frames), reloads for120
frames, and verifies the saved timer resumes before one body retires. Parent
runs serial builds/XEMU. No host input, images, PC runtime or route playthrough.
"""
import argparse
import datetime
import json
from pathlib import Path
import struct

from xemu_corpse_lifecycle import ACTORS, SLAY, SYMBOLS as BASE_SYMBOLS, prepare_level, during_fade, saved_retired_actor
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest, address
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SAVE_FRAMES, LOAD_FRAMES, LOAD_PROBE = 92, 120, 10
SYMBOLS = dict(BASE_SYMBOLS, rf_scene_corpse_lifetime_checkpoint=8,
               rf_scene_corpse_checkpoint=8, rf_scene_live_death_audio=4,
               rf_scene_weapon_drops=8, rf_scene_world_load_reject=3,
               rf_scene_npc_checkpoint_reject_state=6,rf_scene_corpse_unsettled_checkpoint=4,
               rf_scene_actor_landing=8)


def lifetime_rows(payload):
    if payload[:4] != b'RFWC' or len(payload) < 320:
        raise RuntimeError('Missing ordinary world checkpoint')
    kind, offset, size = struct.unpack_from('<III', payload, 128+10*12)
    if kind != 11 or offset+size > len(payload):
        raise RuntimeError('Invalid vehicle companion bounds')
    data = payload[offset:offset+size]
    if data[:4] != b'RFCL' or len(data) < 16:
        raise RuntimeError('Missing corpse lifetime companion')
    version, inner, count = struct.unpack_from('<III', data, 4)
    if version != 1 or not 0 < count <= 30 or 16+inner+count*48 != len(data):
        raise RuntimeError('Invalid RFCL1 structure')
    result = []
    for i in range(count):
        uid, cls, age, fade, flags, obj, health, temp, classtemp, motion, action, reserved = \
            struct.unpack_from('<IIffIIfffiiI', data, 16+inner+i*48)
        if reserved:raise RuntimeError('Nonzero reserved lifetime word')
        result.append(dict(uid=uid, class_id=cls, age=age, fade=fade, flags=flags,
                           object_flags=obj, health=health, temperature=temp,
                           class_temperature=classtemp, motion=motion, action=action))
    return result


def early_loaded(monitor, mapping):
    result = during_fade(monitor, mapping)
    result['rf_scene_corpse_lifetime_checkpoint'] = words(
        monitor, address(mapping, 'rf_scene_corpse_lifetime_checkpoint'), 8)
    return result


def check_run(run, frames):
    if run['guest_phase'] != 5 or run['frames'] != frames or \
       run['memory_bytes'] != 64*1024*1024 or run['free_pages'] <= 0 or run['player_life'][2]:
        raise RuntimeError('Incomplete stock64MiB run')
    x = run['extra']
    if any(x['rf_scene_live_corpses'][6:]) or x['rf_scene_corpse_lifecycle'][5] or \
       x['rf_scene_corpse_lifetime_checkpoint'][7] or x['rf_scene_corpse_source_retirement'][3]:
        raise RuntimeError('Corpse construction/lifetime/retirement error')


def validate_source(run, payload):
    check_run(run, SAVE_FRAMES)
    x, state = run['extra'], run['checkpoint_state']
    # Retained pre-sample runs still enforce landing1 in successful engine save admission.
    if 'rf_scene_actor_landing' in x and x['rf_scene_actor_landing'][1]!=1:
        raise RuntimeError(f"Source player did not ground before save: {x['rf_scene_actor_landing']}")
    if x['rf_scene_setup_result'] != [1, SLAY, 1, 0] or \
       x['rf_scene_script_slays'][0:2] != [6, 6] or x['rf_scene_script_slays'][5] or \
       state[9] != 1 or state[3] or state[4] != len(payload):
        raise RuntimeError('Six ordinary deaths or source save failed')
    rows = lifetime_rows(payload)
    if [r['uid'] for r in rows] != list(ACTORS) or \
       [r['uid'] for r in rows if r['flags'] & 1] != [ACTORS[0]] or \
       not .25 < rows[0]['fade'] < .7 or not 0 < rows[0]['age'] < 1:
        raise RuntimeError(f'Expected one oldest body partway through its fade: {rows}')
    # Completion follows normal level teardown. RFCL capture admits only real
    # owned bodies; the last gameplay tick must agree with its six saved rows.
    if x['rf_scene_live_corpses'][0:2] != [6,6] or \
       x['rf_scene_live_corpses'][5] or not x['rf_scene_corpse_lifetime_checkpoint'][0]:
        raise RuntimeError('Source save did not retain six actual owned bodies')
    if any(x[n][0] for n in ('campaign_model_owned_count','campaign_model_owned_bytes',
                            'campaign_live_corpse_count','campaign_live_corpse_object_count')):
        raise RuntimeError('Source teardown retained corpse/model resources')
    return rows


def validate(saved, loaded, source_payload, final_payload):
    rows = validate_source(saved, source_payload);check_run(loaded, LOAD_FRAMES)
    x, state, early = loaded['extra'], loaded['checkpoint_state'], loaded['probe']
    if state[8] != 1 or state[0] or state[1] != len(source_payload) or any(x['rf_scene_world_load_reject']):
        raise RuntimeError('Ordinary fresh load failed')
    restored = x['rf_scene_corpse_checkpoint']
    if restored[:4] != [1, 6, 6, 0] or restored[5] or any(x['rf_scene_live_death_audio']) or \
       any(x['rf_scene_script_slays']) or x['rf_scene_weapon_drops'][0]:
        raise RuntimeError('Load lost bodies/poses or replayed death side effects')
    frame = early['rf_scene_profile_stage'][0]
    if not LOAD_PROBE <= frame < 22:raise RuntimeError(f'Missed early restored fade: frame{frame}')
    restored_life = early['rf_scene_corpse_lifetime_checkpoint']
    expected_bits = [struct.unpack('<I', struct.pack('<f', rows[0][k]))[0] for k in ('fade', 'age')]
    if restored_life[1:7] != [1, 1, 6, ACTORS[0], *expected_bits] or restored_life[7]:
        raise RuntimeError(f'Restored lifetime scalar mismatch: {restored_life}')
    if early['campaign_live_corpse_count'] != [6] or early['campaign_model_owned_count'] != [6]:
        raise RuntimeError('Body retired before its saved timer elapsed')
    fade = early['rf_scene_corpse_lifecycle']
    expected_alpha = max(0, int((rows[0]['fade']-frame/60)*255))
    if not fade[2] or fade[6] != ACTORS[0] or not 0 < fade[7] < 255 or \
       abs(fade[7]-expected_alpha) > 18:
        raise RuntimeError(f'Fade restarted or failed to progress from saved value: {fade}, expected about{expected_alpha}')
    if x['rf_scene_live_corpses'][1] != 5 or x['rf_scene_live_corpses'][5] != 1 or \
       x['rf_scene_corpse_source_retirement'][:4] != [1, 0, ACTORS[0], 0] or \
       x['rf_scene_corpse_source_retirement'][9:] != [0,0,1]:
        raise RuntimeError('Restored fade did not retire exactly its source corpse')
    if any(x[n][0] for n in ('campaign_model_owned_count','campaign_model_owned_bytes',
                            'campaign_live_corpse_count','campaign_live_corpse_object_count')):
        raise RuntimeError('Loaded teardown retained corpse/model resources')
    if state[9] != 1 or state[3] or state[4] != len(final_payload):
        raise RuntimeError('Post-retirement ordinary save failed')
    retired = saved_retired_actor(final_payload)
    final_rows = lifetime_rows(final_payload)
    if [r['uid'] for r in final_rows] != list(ACTORS[1:]) or any(r['flags'] & 1 for r in final_rows):
        raise RuntimeError('Final save retained stale or duplicate fading owner')
    return dict(result='PASS', saved_oldest=rows[0], restored_lifetime=restored_life,
                observed_frame=frame, observed_alpha=fade[7], final_actors=retired,
                free_pages=min(saved['free_pages'], loaded['free_pages']),
                limitations='One empty-testbed six-body fade/save/load; no images, carried/burning corpses, timer wrap or moving corpse physics checked.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', type=Path)
    parser.add_argument('--validate-existing', type=Path)
    args = parser.parse_args()
    if args.prepare_only:print(prepare_level(args.prepare_only,slay_delay=1.0));return
    if args.validate_existing:
        f = args.validate_existing
        print(json.dumps(validate(json.loads((f/'save/result.json').read_text()),
            json.loads((f/'load/result.json').read_text()), (f/'save/xbox-world.rfwc').read_bytes(),
            (f/'load/xbox-world.rfwc').read_bytes()), indent=2));return
    require_no_project_xemu(ROOT);hdd = prepare(ROOT, ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder = ROOT/'artifacts/xemu'/('corpse-fade-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    fixture = prepare_level(folder/'level',slay_delay=1.0)
    names = set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original = {n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report = dict(result='FAIL', scope=__doc__)
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(fixture.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<I', SLAY))
        (DISC/'campaign-actor.bin').write_bytes(struct.pack('<I', ACTORS[0]))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved = run_guest(folder,'save',hdd,SAVE_FRAMES,360,capture_world=True,extra_symbols=SYMBOLS)
        report['save'] = saved
        source_payload = (folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_rows'] = validate_source(saved, source_payload)
        (DISC/'campaign-setup.bin').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        # Keep the ordinary save flag: the second endpoint verifies the released
        # body's persistent identity without requesting a third native run.
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(LOAD_FRAMES*48))
        build(folder,'load')
        loaded = run_guest(folder,'load',hdd,LOAD_FRAMES,360,capture_world=True,
                           extra_symbols=SYMBOLS,probe=early_loaded,probe_frame=LOAD_PROBE)
        report['load'] = loaded
        report.update(validate(saved,loaded,source_payload,(folder/'load/xbox-world.rfwc').read_bytes()))
    except Exception as exc:report['error'] = str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        except Exception as exc:
            report['result']='FAIL';report['restore_build_error']=str(exc);raise
        finally:
            report['disc_restored'] = all(((DISC/n).read_bytes() if (DISC/n).exists() else None) == data for n,data in original.items())
            if not report['disc_restored']:report['result'] = 'FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__ == '__main__':main()
