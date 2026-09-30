"""Bounded Xbox NPC burning owner/timer save and fresh load; no images or route."""
import argparse
import datetime
from pathlib import Path
import json
import struct
from xemu_native_world_save import FLAGS,ROOT,DISC,build,run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS={'rf_scene_burning':5,'rf_scene_burning_restored':7,'rf_scene_burning_live':8,
         'rf_scene_npc_checkpoint_reject_state':6,'rf_scene_world_load_reject':3}
UID=10318

def f(word):return struct.unpack('<f',struct.pack('<I',word))[0]
def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def validate_saved(saved,payload):
    state=saved['checkpoint_state']
    if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:
        raise RuntimeError(f'Save failed: {state}, {saved["extra"]}')
    off,size=struct.unpack_from('<II',payload,128+16*12+4)
    block=payload[off:off+size]
    if block[:4]!=b'RFAP' or struct.unpack_from('<III',block,4)!=(4,1,0):
        raise RuntimeError('Expected RFAP4 one burn row')
    row=struct.unpack_from('<7I',block,16)
    kind,slot,target,source_kind,source_uid,left,phase=row
    if kind!=10 or target!=UID or source_kind!=1 or source_uid or left<=0 or phase>=15:
        raise RuntimeError(f'Invalid saved burn: {row}')
    live=saved['extra']['rf_scene_burning_live']
    if live[0]!=UID or live[3:6]!=[slot+1,left,phase]:
        raise RuntimeError(f'Owner differs from saved burn: {live}')
    return row,list(struct.unpack_from('<4iI',block,16+28))

def validate_burn_continuity(saved,loaded,payload):
    """Validate burn behavior only; preserve/report independent player-death failure."""
    row,pain=validate_saved(saved,payload)
    _,_,_,_,_,left,phase=row
    state=saved['checkpoint_state'];state2=loaded['checkpoint_state']
    restored=loaded['extra']['rf_scene_burning_restored']
    live=saved['extra']['rf_scene_burning_live'];health=f(live[1]);class_health=f(live[2])
    if loaded['guest_phase']&0x80000000 or state2[8]!=1 or state2[0] or state2[1]!=state[4]:
        raise RuntimeError(f'Load failed: {state2}, {loaded["extra"]}')
    if saved['frames']!=70 or loaded['frames']!=270 or any(r['memory_bytes']!=64*1024*1024 for r in (saved,loaded)):
        raise RuntimeError('Missing expected bounded stock64MiB runs')
    if restored[:6]!=[1,UID,1,0,left,phase] or f(restored[6])!=health:
        raise RuntimeError(f'Burn restoration mismatch: {restored}')
    available_pulses=(phase+left)//15
    # Installed elite fire factor1; its authored instance health is below class
    # health, so burning may kill before the full lifetime expires.
    pulse_damage=f32(class_health*f32(.25/6.5));pulses=0;expected=health
    while pulses<available_pulses and expected>0:
        expected=f32(expected-pulse_damage);pulses+=1
        if 0<=expected<=.5:expected=-.1
    died=int(expected<=0)
    final=loaded['extra']['rf_scene_burning_live'];burning=loaded['extra']['rf_scene_burning']
    if burning!=[0,pulses,1,died,0] or final[3:6]!=[0,0,0] or final[6]!=0xffffffff or abs(f(final[1])-expected)>.001:
        raise RuntimeError(f'Burn continuation mismatch: {burning}, {final}, expectedhealth {expected}')
    player_death=any(r['player_life'][2] for r in (saved,loaded))
    return dict(result='PASS_BURN_CONTINUITY',scope='Saved NPC burn owner, pulse cadence, damage and terminal retirement only',
        saved_burn=row,saved_pain=pain,remaining_pulses=pulses,burn_death=bool(died),final_health=f(final[1]),
        free_pages=min(saved['free_pages'],loaded['free_pages']),player_death_observed=player_death,
        player_life={name:r['player_life'] for name,r in (('save',saved),('load',loaded))},
        caveat='The native generic harness rejected a player-death observation; this scoped burn validation does not override that failure or establish player survival.' if player_death else 'No player-death observation in these captured results.')

def validate_existing(folder):
    original=json.loads((folder/'report.json').read_text())
    if not original.get('disc_restored'):raise RuntimeError('Disc restoration not confirmed')
    saved=json.loads((folder/'save/result.json').read_text())
    loaded=json.loads((folder/'load/result.json').read_text())
    result=validate_burn_continuity(saved,loaded,(folder/'save/xbox-world.rfwc').read_bytes())
    result.update(native_rerun=False,original_report_result=original['result'],original_report_preserved=True,disc_restored=True)
    (folder/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(folder/'validation.json',result['result'],flush=True)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--validate-existing',type=Path,help='Validate recorded burn evidence only without rerunning or changing generic native gates')
    args=parser.parse_args()
    if args.validate_existing:
        validate_existing(args.validate_existing);return
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('burning-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','campaign-burning-save.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':'Ordinary NPC fire damage and burn-owner save/load','phases':{}}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64,b'\0')+b'L8S4.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-actor.bin').write_bytes(struct.pack('<I',UID))
        (DISC/'campaign-burning-save.bin').write_bytes(struct.pack('<I',UID))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(70*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,70,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved;state=saved['checkpoint_state']
        if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:raise RuntimeError(f'Save failed: {state}, {saved["extra"]}')
        payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_burn'],_=validate_saved(saved,payload)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(270*48))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,270,420,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['load']=loaded
        result=validate_burn_continuity(saved,loaded,payload)
        report.update(result='PASS',burn_validation=result)
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        build(folder,'restore')
        report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
        if not report['disc_restored']:report['result']='FAIL'
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')

if __name__=='__main__':main()
