"""Bounded Xbox ordinary save/load of an NPC-bound remote, then detonation."""
import datetime
import json
import struct
from xemu_native_world_save import FLAGS, ROOT, DISC, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS = {'rf_scene_remote':8, 'rf_scene_remote_restored':6,
           'rf_scene_remote_shield':4, 'rf_scene_world_load_reject':3,
           'rf_scene_npc_checkpoint_reject_state':6, 'rf_scene_rocket_blast':8}

def replay(frames, detonate=False):
    rows=[]
    for frame in range(frames):
        values=[0]*12
        if detonate:
            values[10]=int(frame==10)  # charge -> detonator
            values[8]=int(frame==30)
        rows.append(struct.pack('<5f7I',*values))
    return b'RFI6'+struct.pack('<I',48)+b''.join(rows)

def main():
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('remote-host-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':'NPC-bound remote ordinary Xbox save/load and detonation','phases':{}}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64,b'\0')+b'L8S4.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-setup.bin').write_bytes(struct.pack('<I',10358))
        (DISC/'campaign-nano-shield.bin').write_bytes(struct.pack('<2I',8359,3))
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(80))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,80,360,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved
        state=saved['checkpoint_state']
        if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:raise RuntimeError(f'Save failed: {state}')
        payload=(folder/'save/xbox-world.rfwc').read_bytes()
        off,size=struct.unpack_from('<II',payload,128+9*12+4)
        remote=payload[off:off+size]
        if remote[:4]!=b'RFRM' or struct.unpack_from('<I',remote,12)[0]!=1:raise RuntimeError('Expected one saved remote')
        host=struct.unpack_from('<I',remote,48+12)[0]
        position=struct.unpack_from('<3f',remote,48+16)
        report['saved_host']=host;report['saved_position']=position
        if host!=8359:raise RuntimeError('Wrong saved remote host')
        (DISC/'campaign-nano-shield.bin').unlink()
        (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(80,True))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,80,360,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['load']=loaded
        state2=loaded['checkpoint_state'];restored=loaded['extra']['rf_scene_remote_restored'];live=loaded['extra']['rf_scene_remote']
        if loaded['guest_phase']&0x80000000 or state2[8]!=1 or state2[0] or state2[1]!=state[4]:raise RuntimeError(f'Load failed: {state2}, {loaded["extra"]}')
        if restored[:3]!=[1,1,8359]:raise RuntimeError(f'Attachment not restored: {restored}')
        actual=struct.unpack('<3f',struct.pack('<3I',*restored[3:]))
        if any(abs(a-b)>.001 for a,b in zip(actual,position)):raise RuntimeError(f'Saved attachment pose changed: {position}, {actual}')
        if live[1:5]!=[0,0,1,0] or loaded['extra']['rf_scene_rocket_blast'][0]!=1:raise RuntimeError(f'Restored detonation failed: {live}')
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
