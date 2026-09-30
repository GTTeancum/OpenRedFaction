"""Bounded Xbox ordinary save/load of an active Fusion flight and reload."""
import argparse
from pathlib import Path
from xemu_ai_projectile_save import fixture
import datetime
import json
import struct
from xemu_native_world_save import FLAGS, ROOT, DISC, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS = {'rf_scene_fusion_projectiles':5,'rf_scene_fusion_restored':6,
           'rf_scene_fusion_motion':6,'rf_scene_player_ammo':8,'rf_scene_combat':8,
           'rf_scene_world_load_reject':3,'rf_scene_world_player_probe':3}

def replay(frames):
    return b'RFI6'+struct.pack('<I',48)+bytes(frames*48)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--load-run',type=Path,help='Reuse a captured source save instead of repeating the save session')
    args=parser.parse_args()
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('fusion-flight-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':'Active Fusion flight and reload ordinary Xbox save/load','phases':{}}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64,b'\0')+b'L8S4.rfl'.ljust(64,b'\0'))
        (DISC/'player-control.flag').write_bytes(b'')
        if args.load_run:
            saved=json.loads((args.load_run/'save/result.json').read_text())
            payload=(args.load_run/'save/xbox-world.rfwc').read_bytes()
            report['source_run']=str(args.load_run)
        else:
            (DISC/'campaign-special-save.bin').write_bytes(struct.pack('<I',3))
            (DISC/'world-hdd-save.flag').write_bytes(b'1')
            (DISC/'player-replay.bin').write_bytes(replay(80))
            build(folder,'save')
            saved=run_guest(folder,'save',hdd,80,360,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
            payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['phases']['save']=saved
        state=saved['checkpoint_state']
        if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:raise RuntimeError(f'Save failed: {state}')
        off,size=struct.unpack_from('<II',payload,128+16*12+4)
        component=payload[off:off+size]
        if component[:4]!=b'RFAP' or struct.unpack_from('<3I',component,4)!=(2,1,0):raise RuntimeError('Expected RFAP2 with one Fusion row')
        row=component[16:];kind=struct.unpack_from('<I',row)[0]
        position=struct.unpack_from('<3f',row,16);velocity=struct.unpack_from('<3f',row,28)
        remaining=struct.unpack_from('<d',row,48)[0];cooldown,reload=struct.unpack_from('<2I',row,60)
        report['saved_fusion']={'kind':kind,'position':position,'velocity':velocity,'remaining':remaining,'cooldown':cooldown,'reload':reload}
        ammo=saved['extra']['rf_scene_player_ammo']
        if kind!=6 or not 0<reload<60 or not 0<cooldown<60 or ammo[1:3]!=[1,0] or saved['extra']['rf_scene_fusion_projectiles'][3]!=1:raise RuntimeError(f'Active shot/reload not saved: {report["saved_fusion"]}, {ammo}')
        (DISC/'campaign-special-save.bin').write_bytes(struct.pack('<I',4))
        (DISC/'world-hdd-save.flag').unlink(missing_ok=True)
        if args.load_run:
            (DISC/'world-fixture.0').write_bytes(fixture(payload))
            (DISC/'world-fixture-load.flag').write_bytes(b'1')
        else:(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(60))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,60,360,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['load']=loaded
        state2=loaded['checkpoint_state'];restored=loaded['extra']['rf_scene_fusion_restored']
        if loaded['guest_phase']&0x80000000 or state2[8]!=1 or state2[0] or state2[1]!=state[4]:raise RuntimeError(f'Load failed: {state2}, {loaded["extra"]}')
        if restored[:3]!=[1,cooldown,reload] or struct.unpack('<3f',struct.pack('<3I',*restored[3:]))!=position:raise RuntimeError(f'Flight/timers changed at restore: {restored}')
        motion=loaded['extra']['rf_scene_fusion_motion'];flights=loaded['extra']['rf_scene_fusion_projectiles'];ammo=loaded['extra']['rf_scene_player_ammo']
        if flights[0] or ammo[1:3]!=[0,1] or loaded['extra']['rf_scene_combat'][6]:raise RuntimeError(f'Reload/ammo continuity failed: {flights}, {ammo}')
        if motion[1]:
            actual=struct.unpack('<3f',struct.pack('<3I',*motion[2:5]))
            expected=[position[i]+velocity[i]*motion[0]/60 for i in range(3)]
            time_left=struct.unpack('<f',struct.pack('<I',motion[5]))[0]
            if any(abs(a-b)>.002 for a,b in zip(actual,expected)) or abs(time_left-(remaining-motion[0]/60))>.0001:raise RuntimeError(f'Flight did not advance from saved state: {actual}, {expected}')
            report['continued_position']=actual
        elif sum(flights[1:3])!=1:raise RuntimeError(f'Flight disappeared without terminal event: {flights}')
        report['result']='PASS'
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        build(folder,'restore')
        report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
        if not report['disc_restored']:report['result']='FAIL'
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')

if __name__=='__main__':main()
