"""Bounded Xbox ordinary save/load of an damaged shield and Fusion ownership."""
import datetime
import json
import struct
from xemu_native_world_save import FLAGS, ROOT, DISC, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

SYMBOLS = {'rf_scene_special_save_probe':25, 'rf_scene_player_shield':4,
           'rf_scene_fusion_projectiles':5, 'rf_scene_player_ammo':8,
           'rf_scene_world_load_reject':3, 'rf_scene_world_player_probe':3}

def replay(frames):
    return b'RFI6'+struct.pack('<I',48)+bytes(frames*48)

def main():
    require_no_project_xemu(ROOT)
    hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('special-weapon-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(parents=True)
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names}
    report={'result':'FAIL','scope':'Damaged Riot Shield and Fusion ownership ordinary Xbox save/load','phases':{}}
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'campaign-spawn.flag').write_bytes(b'')
        (DISC/'campaign-level.bin').write_bytes(b'levels2.vpp'.ljust(64,b'\0')+b'L8S4.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-special-save.bin').write_bytes(struct.pack('<I',1))
        (DISC/'player-control.flag').write_bytes(b'')
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(80))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,80,360,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved
        state=saved['checkpoint_state']
        if saved['guest_phase']&0x80000000 or state[9]!=1 or state[3]:raise RuntimeError(f'Save failed: {state}')
        payload=(folder/'save/xbox-world.rfwc').read_bytes()
        off,size=struct.unpack_from('<II',payload,128+8*12+4)
        modes=payload[off:off+size]
        if modes[:4]!=b'RFWM' or struct.unpack_from('<I',modes,4)[0]!=2:raise RuntimeError('Expected RFWM2')
        life=struct.unpack_from('<f',modes,28)[0];report['saved_shield_life']=life
        staged=saved['extra']['rf_scene_special_save_probe']
        f=lambda word:struct.unpack('<f',struct.pack('<I',word))[0]
        if staged[0]!=1 or f(staged[2])!=life or life!=f(staged[1])*.5 or staged[3:7]!=[1,1,11,11]:raise RuntimeError(f'Shield not damaged before save: {staged}')
        (DISC/'campaign-special-save.bin').write_bytes(struct.pack('<I',2))
        (DISC/'world-hdd-save.flag').unlink()
        (DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(replay(90))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,90,360,snapshot=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['load']=loaded
        state2=loaded['checkpoint_state'];staged=loaded['extra']['rf_scene_special_save_probe']
        if loaded['guest_phase']&0x80000000 or state2[8]!=1 or state2[0] or state2[1]!=state[4]:raise RuntimeError(f'Load failed: {state2}, {loaded["extra"]}')
        if staged[0]!=2 or f(staged[1])!=life or f(staged[2])!=life*.5 or staged[3:7]!=[1,1,11,11]:raise RuntimeError(f'Shield durability not continued: {staged}')
        if staged[11:13]!=[1,0] or staged[14]==11 or loaded['extra']['rf_scene_player_shield'][:2]!=[2,1]:raise RuntimeError(f'Shield did not break/remove after load: {staged}')
        if loaded['extra']['rf_scene_fusion_projectiles'][0]!=1 or loaded['extra']['rf_scene_player_ammo'][2]!=0:raise RuntimeError('Restored Fusion did not fire/spend its shell')
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
