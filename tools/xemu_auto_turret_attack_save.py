"""Xbox ordinary save/fresh-load of NPC Attack against a generated AutoHead.

Real L3S2 guard941 and Auto Turret base1994 in CTF06, transforms only. A
process-local Attack request targets the real generated handle; a finite source
fire deadline crosses save60/load60. Set_AI_Mode(Catatonic) suspends the head's
auto-fire and persists normally. No fake head, forced damage or host input.
"""
import datetime
import hashlib
import io
import json
import struct
from xemu_auto_turret import BASE, prepare_level as prepare_base, SYMBOLS as AUTO_SYMBOLS, U, F, f
from xemu_npc_turret_seat import record_details
from xemu_turret_combat import entity_rows
from check_ai_projectile_ordinary import archive
from build_fragment_platform_fixture import read_entry
from inspect_levels import inspect
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare

ACTOR=941
SAVE_FRAMES,LOAD_FRAMES=60,60
SYMBOLS=dict(AUTO_SYMBOLS,rf_scene_turret_generated_attack_test=15,
    rf_scene_turret_generated_attack_restore=12,rf_scene_turret_generated_restore_probe=32,
    rf_scene_script_attack=12,rf_scene_npc_checkpoint_reject_state=6,rf_scene_world_load_reject=3,
    rf_scene_turret_generated_death_probe=10,rf_scene_turret_death_effects=8)


def prepare_level(folder):
    path=prepare_base(folder);original=read_entry(path,'ctf06.rfl')
    source=read_entry(ROOT/'Installed_Game/levels1.vpp','L3S2.rfl')
    actor=next(r for r in entity_rows(source) if r['uid']==ACTOR)
    details=record_details(actor)
    if actor['name']!='guard1' or details['seat_host_uid']!=-1 or b'12mm handgun' not in actor['raw']:
        raise RuntimeError('Expected authored unseated pistol guard941')
    base=entity_rows(original)[0];position=struct.unpack_from('<3f',base['raw'],base['transform'])
    staged=(position[0]+3,position[1],position[2])
    raw=bytearray(actor['raw']);at=actor['transform']
    raw[at:at+48]=F(*staged,-1,0,0,0,0,1,0,1,0)
    payload=U(2)+raw+base['raw']
    meta=inspect(io.BytesIO(original),dict(offset=0,size=len(original),name='ctf06.rfl'))
    out=bytearray(original[:meta['sections'][0]['offset']]);offsets={}
    for section in meta['sections']:
        kind=int(section['type'],16);offsets[kind]=len(out)
        data=payload if kind==0x30000 else original[section['offset']+8:section['offset']+8+section['size']]
        out+=U(kind,len(data))+data
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    if [(r['uid'],r['name']) for r in entity_rows(out)]!=[(ACTOR,'guard1'),(BASE,'Auto Turret')]:
        raise RuntimeError('Fixture changed authored actor population')
    archive(path,[('ctf06.rfl',out)])
    recipe=json.loads((folder/'recipe.json').read_text())
    recipe.update(scope=__doc__,attacker_uid=ACTOR,attacker_position=staged,attacker_details=details,
        attacker_source_sha256=hashlib.sha256(actor['raw']).hexdigest(),source_frames=SAVE_FRAMES,
        load_frames=LOAD_FRAMES,staged_order_frame=1,staged_first_fire_frame=106,
        staged_head_ai_mode=1)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n');return path


def component(payload,kind):
    if len(payload)<320 or payload[:4]!=b'RFWC':raise RuntimeError('Missing ordinary world checkpoint')
    k,offset,size=struct.unpack_from('<III',payload,128+(kind-1)*12)
    if k!=kind or offset<320 or offset+size>len(payload):raise RuntimeError('Invalid component bounds')
    return payload[offset:offset+size]


def saved_attack(payload):
    block=component(payload,2)
    if block[:4]!=b'RFNC' or struct.unpack_from('<I',block,4)[0]!=11 or struct.unpack_from('<I',block,16)[0]!=2:
        raise RuntimeError('Expected RFNC11 with authored guard/base only')
    at=64;found=None;uids=[]
    for _ in range(2):
        if at+600>len(block):raise RuntimeError('Truncated NPC row')
        row=block[at:];uid=struct.unpack_from('<I',row)[0];uids.append(uid)
        animation,move,combat,shots=(struct.unpack_from('<I',row,o)[0] for o in (540,564,568,588))
        span=600+move+combat+shots*24+animation
        if at+span>len(block):raise RuntimeError('Truncated NPC extensions')
        if uid==ACTOR:
            if combat!=40:raise RuntimeError('Missing active Attack combat extension')
            words=list(struct.unpack_from('<10I',row,600+move))
            if words[:2]!=[3,BASE] or words[6:9]!=[0,0,0] or not 0<words[3]<LOAD_FRAMES:
                raise RuntimeError(f'Generated-head target discriminator/deadline differs: {words}')
            found=dict(active=words[0],base_uid=words[1],burst=words[2],remaining=words[3],
                reload_remaining=words[4],reload_weapon=words[5],spread_rng=words[9])
        at+=span
    if at!=len(block) or uids!=[ACTOR,BASE] or found is None:raise RuntimeError('Unexpected RFNC owner rows')
    turret=component(payload,11)
    if len(turret)!=232 or turret[:4]!=b'RFTU' or struct.unpack_from('<III',turret,4)!=(2,0,1):
        raise RuntimeError('Expected live RFTU2 generated-head companion')
    if struct.unpack_from('<II',turret,16)!=(BASE,1) or struct.unpack_from('<I',turret,40)[0]:
        raise RuntimeError('Saved generated key/dead state differs')
    if struct.unpack_from('<i',turret,60)[0]!=1:
        raise RuntimeError('Head Catatonic action did not persist')
    found['head_health']=struct.unpack_from('<f',turret,32)[0]
    if found['head_health']!=60:raise RuntimeError('Source damaged the head before its staged fire deadline')
    return found


def check(result,frames):
    if result['guest_phase']!=5 or result['frames']!=frames or result['memory_bytes']!=64*1024*1024 or result['free_pages']<=0 or result['player_life'][2]:
        raise RuntimeError('Incomplete bounded living-player stock64MiB run')
    x=result['extra'];p=x['rf_scene_turret_generated_attack_test']
    if f(x['rf_scene_pickup_vitals'][0])<=0 or x['rf_scene_turret_shots'][0]:
        raise RuntimeError('Player died or Catatonic head fired')
    if p[1:3]!=[ACTOR,BASE] or p[3]==p[4] or 0xffffffff in p[3:5] or p[12] or f(p[14])<=0:
        raise RuntimeError(f'Invalid real attacker/head identity or fixture error: {p}')
    if p[5:7]!=[1,1] or p[13] or f(p[9])<=0 or f(p[10])!=80:
        raise RuntimeError(f'Attack lost its live generated head or targeted/damaged its base: {p}')
    if x['rf_scene_enemy_combat'][7] or x['rf_scene_turret_owners'][7] or any(x['rf_scene_turret_generated_stats'][5:]):
        raise RuntimeError('Combat/owner/generated death error')
    for name in ('rf_scene_turret_generated_death_probe','rf_scene_turret_death_effects','rf_scene_turret_test'):
        if any(x[name]):raise RuntimeError(f'Unexpected terminal/synthetic contact path: {name}')
    return x,p


def validate_source(saved,payload):
    x,p=check(saved,SAVE_FRAMES);state=saved['checkpoint_state'];row=saved_attack(payload)
    if state[9]!=1 or state[3] or state[4]!=len(payload) or p[0]!=1 or any(x['rf_scene_turret_generated_attack_restore']):
        raise RuntimeError('Source order/save failed or unexpectedly restored')
    if x['rf_scene_turret_owners'][2] or f(p[9])!=60:raise RuntimeError('Head was damaged before the saved deadline')
    return row


def validate(saved,loaded,payload):
    row=validate_source(saved,payload);x,p=check(loaded,LOAD_FRAMES);state=loaded['checkpoint_state']
    r=x['rf_scene_turret_generated_attack_restore']
    if state[8]!=1 or state[0] or state[1]!=len(payload) or any(x['rf_scene_world_load_reject']) or p[0]:
        raise RuntimeError('Ordinary load failed or Attack was reissued by the fixture')
    if r[:6]!=[1,ACTOR,BASE,p[4],1,1] or r[6:]!=[row['remaining'],row['reload_remaining'],row['burst'],0,row['remaining'],row['spread_rng']]:
        raise RuntimeError(f'Generated target/cadence/RNG did not restore exactly: {r}, {row}')
    if not x['rf_scene_enemy_combat'][2] or not x['rf_scene_enemy_combat'][3] or not x['rf_scene_turret_owners'][2] or not 0<f(p[9])<row['head_health']:
        raise RuntimeError('Restored NPC Attack did not fire and damage the real generated head')
    return dict(result='PASS',saved_attack=row,restored_attack=r,actor_uid=ACTOR,base_uid=BASE,
        initial_head_health=row['head_health'],loaded_head_health=f(p[9]),base_health=f(p[10]),
        loaded_npc_shots=x['rf_scene_enemy_combat'][2],loaded_head_damage_requests=x['rf_scene_turret_owners'][2],
        loaded_attack_reissues=0,free_pages=min(saved['free_pages'],loaded['free_pages']),
        limitations='One staged generated-head Attack save/fresh-load; native aiming/weapon damage required, no campaign or visual/audio claim.')


def main():
    require_no_project_xemu(ROOT);hdd=prepare(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    folder=ROOT/'artifacts/xemu'/('auto-turret-attack-save-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    path=prepare_level(folder/'level')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'player-control.flag','scene-fixture.vpp','campaign-auto-attack.bin'}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in names};report=dict(result='FAIL',scope=__doc__,phases={})
    try:
        for n in names:(DISC/n).unlink(missing_ok=True)
        (DISC/'scene-fixture.vpp').write_bytes(path.read_bytes())
        (DISC/'campaign-level.bin').write_bytes(b'scene-fixture.vpp'.ljust(64,b'\0')+b'ctf06.rfl'.ljust(64,b'\0'))
        (DISC/'campaign-spawn.flag').write_bytes(b'');(DISC/'player-control.flag').write_bytes(b'')
        (DISC/'campaign-auto-attack.bin').write_bytes(U(BASE,ACTOR,1))
        (DISC/'world-hdd-save.flag').write_bytes(b'1')
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(SAVE_FRAMES*48))
        build(folder,'save')
        saved=run_guest(folder,'save',hdd,SAVE_FRAMES,420,capture_world=True,extra_symbols=SYMBOLS,allow_guest_error=True)
        report['phases']['save']=saved
        if saved['guest_phase']!=5:
            raise RuntimeError(f"Source save rejected: {saved['checkpoint_state']}; player health={f(saved['extra']['rf_scene_pickup_vitals'][0])}")
        payload=(folder/'save/xbox-world.rfwc').read_bytes()
        report['saved_attack']=validate_source(saved,payload)
        (DISC/'world-hdd-save.flag').unlink();(DISC/'world-hdd-load.flag').write_bytes(b'1')
        (DISC/'campaign-auto-attack.bin').write_bytes(U(BASE,ACTOR,2))
        (DISC/'player-replay.bin').write_bytes(b'RFI6'+U(48)+bytes(LOAD_FRAMES*48))
        build(folder,'load')
        loaded=run_guest(folder,'load',hdd,LOAD_FRAMES,420,snapshot=True,extra_symbols=SYMBOLS)
        report['phases']['load']=loaded;report.update(validate(saved,loaded,payload))
    except Exception as exc:report['error']=str(exc);raise
    finally:
        for n,data in original.items():
            if data is None:(DISC/n).unlink(missing_ok=True)
            else:(DISC/n).write_bytes(data)
        try:build(folder,'restore')
        finally:
            report['disc_restored']=all(((DISC/n).read_bytes() if (DISC/n).exists() else None)==data for n,data in original.items())
            if not report['disc_restored']:report['result']='FAIL'
            (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(folder,report['result'],flush=True)
        if not report['disc_restored']:raise RuntimeError('Disc restoration failed')


if __name__=='__main__':main()
