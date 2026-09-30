"""Bounded stock-64-MiB flame controller/canister ordinary save and reload.

Uses the existing enemy-free CTF06 grant fixture, no route or images. Each
mode is one source save and one fresh load. No host input or PC executable.
"""
import argparse
import datetime
import json
import struct
from check_flame_pickup import prepare as prepare_level
from xemu_native_world_save import FLAGS, ROOT, DISC, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS={'rf_scene_flame_canister':5,'rf_scene_flame_restored':9,
         'rf_scene_flame_visual':6,'rf_scene_player_ammo':8,
         'rf_scene_world_load_reject':3,'rf_scene_world_player_probe':3}

def replay(frames,mode,loading=False):
    rows=[]
    for frame in range(frames):
        primary=(mode=='stream' and (loading or frame>=120)) or (mode=='reload' and not loading and 120<=frame<145)
        reload=mode=='reload' and not loading and frame==155
        cycle=not loading and frame==30
        alternate=mode in ('pending','canister') and not loading and frame==120
        rows.append(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(primary),int(reload),int(cycle),int(alternate)))
    return b'RFI6'+struct.pack('<I',48)+b''.join(rows)

def rows(payload):
    off,size=struct.unpack_from('<II',payload,128+16*12+4)
    block=payload[off:off+size]
    if block[:4]!=b'RFAP' or struct.unpack_from('<I',block,4)[0]!=3:raise RuntimeError('Expected RFAP3')
    count=struct.unpack_from('<I',block,8)[0]
    return [block[16+i*112:16+(i+1)*112] for i in range(count)]

def validate_stream(saved,loaded,values,load_frames):
    before=saved['extra']['rf_scene_player_ammo'];after=loaded['extra']['rf_scene_player_ammo']
    # Scene publishes its startup world load at the end of frame0;
    # remaining frames1..N-1 tick the restored stream.
    resumed_frames=load_frames-1
    consumed=(values[5]+100*resumed_frames)//120
    if after[1]!=before[1] or after[2]!=before[2]-consumed or loaded['extra']['rf_scene_flame_visual'][0]!=resumed_frames:
        raise RuntimeError(f'Stream did not continue: {before}, {after}')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode',choices=('stream','pending','canister','reload'),default='canister')
    args=parser.parse_args();mode=args.mode
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('flame-save-'+mode+'-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    prepare_level(folder/'level',1)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':'Flame '+mode+' ordinary Xbox save/load','phases':{}}
    source_frames={'stream':160,'pending':180,'canister':230,'reload':170}[mode]
    load_frames=30 if mode=='stream' else 240
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes((folder/'level/flame/game/levelsm.vpp').read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<II',910540,910541))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(source_frames,mode))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,source_frames,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved
        state=saved['checkpoint_state']
        if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:raise RuntimeError(f'Save failed: {state}')
        payload=(folder/'save/xbox-world.rfwc').read_bytes()
        data=rows(payload)
        controllers=[r for r in data if struct.unpack_from('<I',r)[0]==9]
        flights=[r for r in data if struct.unpack_from('<I',r)[0]==8]
        if len(controllers)!=1:raise RuntimeError('Missing flame controller')
        values=list(struct.unpack_from('<12I',controllers[0],16))
        report['saved_controller']=values;report['saved_flights']=len(flights)
        if mode=='stream' and (values[0]!=2 or not values[5] or not values[8]):raise RuntimeError('No active fractional-fuel stream')
        if mode=='pending' and (not values[10] or flights):raise RuntimeError('No pending throw')
        if mode=='canister' and len(flights)!=1:raise RuntimeError('No active canister')
        if mode=='reload' and not values[2]:raise RuntimeError('No active reload')
        (DISC/'campaign-setup.bin').unlink()
        (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(load_frames,mode,True))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,load_frames,420,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['load']=loaded
        state2=loaded['checkpoint_state'];restored=loaded['extra']['rf_scene_flame_restored']
        expected=[len(flights),values[0],values[2],values[5],values[7],values[10],values[11],values[9],values[8]]
        if loaded['guest_phase']&0x80000000 or state2[8]!=1 or state2[0] or state2[1]!=state[4]:raise RuntimeError(f'Load failed: {state2}')
        if restored!=expected:raise RuntimeError(f'Flame state lost: {restored}, expected {expected}')
        before=saved['extra']['rf_scene_player_ammo'];after=loaded['extra']['rf_scene_player_ammo']
        canister=loaded['extra']['rf_scene_flame_canister']
        if mode=='stream':
            validate_stream(saved,loaded,values,load_frames)
        elif mode=='reload':
            if after[2]!=100 or after[1]!=before[1]-100:raise RuntimeError(f'Reload did not continue: {before}, {after}')
        else:
            if canister[:4]!=[0,int(mode=='pending'),1,0]:raise RuntimeError(f'Canister duplicated or disappeared: {canister}')
            if after[1:3]!=[0,100]:raise RuntimeError(f'Tank replacement wrong: {after}')
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
