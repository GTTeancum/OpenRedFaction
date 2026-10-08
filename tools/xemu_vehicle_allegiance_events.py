"""Bounded stock64MiB original L1S3 Set_Friendliness/source/save/fresh-load.

Original APC26/9627 and Set_Friendliness9619/9636 retain exact source bytes.
Both genuine events write0; this is not an opposite-team toggle. An isolated
Delay/Invert sends OFF before a later ON, then ordinary save/load preserves
APC26's changed affiliation and the pending original3-second repeat. No
NPCs, vehicle combat, route progression, images, host input or guest writes.
"""
import argparse
import datetime
import io
import json
from pathlib import Path
import struct
from build_fragment_platform_fixture import U, read_entry
from check_ai_projectile_ordinary import archive
from inspect_levels import inspect
from xemu_npc_actor_interception import command
from xemu_turret_combat import entity_rows
from xemu_vehicle_npc_push import actor_details, event_rows, section_payload
from xemu_vehicle_rotating_support import sha, f, preserve_launch_binaries, checkpoint_sections
from xemu_native_world_save import ROOT, DISC, FLAGS, build, run_guest
from xemu_session_guard import require_no_project_xemu
from xemu_world_hdd import prepare as prepare_hdd

START, OFF, ON_TIMER = 917000, 917001, 917002
CONFIG = 'campaign-vehicle-allegiance.bin'
FRAMES = (0, 20, 40, 58, 60, 78, 100, 118, 119)
EVENTS = (9619, 9636, START, OFF, ON_TIMER)
OBS = 'rf_scene_vehicle_allegiance_samples'
SYMBOLS = {OBS: 9 * 128, 'rf_scene_vehicle_allegiance_fixture': 8,
    'rf_scene_vehicle_allegiance': 8, 'rf_scene_setup_result': 4,
    'rf_scene_world_load_reject': 3, 'rf_scene_vehicle_damage': 8,
    'rf_scene_passive_damage': 8, 'rf_scene_enemy_combat': 8}
NONE = 0xffffffff


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def event_payload(e):
    raw=e['raw']; at=4
    length=struct.unpack_from('<H', raw, at)[0]; at+=2+length+12
    length=struct.unpack_from('<H', raw, at)[0]; at+=2+length
    return dict(header=raw[at], delay=struct.unpack_from('<f', raw, at+1)[0],
        flags=list(raw[at+5:at+7]), words=list(struct.unpack_from('<2I', raw, at+7)))


def prepare_level(folder):
    folder.mkdir(parents=True, exist_ok=True)
    original=read_entry(ROOT/'Installed_Game/levels1.vpp', 'L1S3.rfl')
    meta=inspect(io.BytesIO(original), dict(offset=0, size=len(original), name='L1S3.rfl'))
    owners={r['uid']:r for r in entity_rows(original)}
    events={r['uid']:r for r in event_rows(section_payload(original, meta, 0x600))}
    details={str(uid):actor_details(owners[uid]) for uid in (26,9627)}
    require(details['26']['friendliness']==1 and details['9627']['friendliness']==0 and
        details['26']['health']==900 and details['9627']['health']==5000, 'Original owner metadata changed')
    source_events={}
    for uid, delay, links in ((9619,0,[26,9449,9452,9454,9455]), (9636,3,[26,9639])):
        e=events[uid]; d=event_payload(e)
        require(e['type']=='Set_Friendliness' and e['links']==links and
            d==dict(header=1,delay=delay,flags=[0,0],words=[0,0]), 'Original event semantics changed')
        source_events[str(uid)]=dict(**d,links=links,sha256=sha(e['raw']))
    # OFF must not change authored1; only the later ordinary ON changes it.
    rows=[events[uid]['raw'] for uid in (9619,9636)]+[
        command(START,'Delay','allegiance_start',(OFF,9636,ON_TIMER)),
        command(OFF,'Invert','allegiance_off',(9619,)),
        command(ON_TIMER,'Delay','allegiance_on',(9619,),delay=.75)]
    replacements={0x30000:U(2)+b''.join(owners[uid]['raw'] for uid in (26,9627)),
        0x600:U(len(rows))+b''.join(rows),0x3000:U(0),0x40000:U(0),0x50000:U(0),0x60000:U(0)}
    out=bytearray(original[:meta['sections'][0]['offset']]); offsets={};preserved={}
    for section in meta['sections']:
        kind=int(section['type'],16);data=section_payload(original,meta,kind);payload=replacements.get(kind,data)
        if kind not in replacements:preserved[hex(kind)]=sha(data)
        offsets[kind]=len(out);out+=U(kind,len(payload))+payload
    struct.pack_into('<II',out,12,offsets[0x70000],offsets[0x1000000])
    checked=inspect(io.BytesIO(out),dict(offset=0,size=len(out),name='L1S3.rfl'))
    require(checked['player_offset_matches'] and checked['info_offset_matches'] and not checked['trailing_bytes'],'Fixture structure failed')
    require([r['raw'] for r in entity_rows(out)]==[owners[u]['raw'] for u in (26,9627)],'Original owner bytes changed')
    decoded=event_rows(section_payload(out,checked,0x600))
    require([r['uid'] for r in decoded]==list(EVENTS) and [r['raw'] for r in decoded]==[bytes(r) for r in rows], 'Event bytes changed')
    for kind,digest in preserved.items():require(sha(section_payload(out,checked,int(kind,16)))==digest,'Preserved section changed')
    path=folder/'scene-fixture.vpp';archive(path,[('L1S3.rfl',out)])
    recipe=dict(original_sha256=sha(original),fixture_sha256=sha(bytes(out)),archive_sha256=sha(path.read_bytes()),
        owners=details,events=source_events,preserved_sections=preserved,
        edits=['keep byte-exact original APC26/9627 and Set_Friendliness9619/9636',
            'remove unrelated actors, items, clutter, controllers and triggers',
            'add isolated ordinary Delay/Invert OFF, delayed ON and pending original9636'],
        limitations=__doc__)
    (folder/'recipe.json').write_text(json.dumps(recipe,indent=2)+'\n')
    return path,recipe


def inputs(level,phase):
    out={'scene-fixture.vpp':level.read_bytes(),'scene-preview.flag':b'',
        'campaign-level.bin':b'scene-fixture.vpp'.ljust(64,b'\0')+b'L1S3.rfl'.ljust(64,b'\0'),
        'campaign-spawn.flag':b'','player-control.flag':b'',CONFIG:U(0x474c4156,phase,0,0),
        'player-replay.bin':b'RFI6'+U(48)+struct.pack('<5f7I',*([0]*12))*120}
    if phase==1:out.update({'campaign-setup.bin':U(START),'world-hdd-save.flag':b'1'})
    else:out['world-hdd-load.flag']=b'1'
    return out


def sample(guest,frame):
    at=FRAMES.index(frame)*128;row=guest['extra'][OBS][at:at+128]
    require(row[:2]==[1,frame] and row[3]==0,'Missing exact native observation '+str(frame))
    return row


def validate(guest,phase,payload=None,previous=None):
    x=guest['extra'];state=guest['checkpoint_state'];rows={f:sample(guest,f) for f in FRAMES}
    require(guest['guest_phase']==5 and guest['frames']==120 and guest['memory_bytes']==67108864 and
        guest['free_pages']>0 and not guest['player_life'][2],'Incomplete living stock64MiB phase')
    require(x['rf_scene_vehicle_allegiance_fixture']==[1,phase,9,26,9627,1,0,0],'Native observer failed')
    require(not any(guest['level_transitions']) and not any(guest['level_request']),'Campaign transition/request occurred')
    for r in rows.values():
        require(r[4]==26 and r[16]==9627 and r[7]==r[19]==1 and len({r[5],r[17],r[27]})==3 and
            all(h not in (0,NONE) and h>>16 for h in (r[5],r[17],r[27])),'Live registered identities changed')
        require(f(r[8])==900 and f(r[20])==5000 and not r[11] and r[15]==r[23]==r[28]==r[101]==0 and
            r[18]==r[31]==0 and r[30]==1 and r[22]&0x4000,'Health, selected source override or owner separation changed')
        require(r[10]==rows[0][10] and r[12:15]==rows[0][12:15],'Scripted allegiance altered passive flags/pose')
        require(r[29]==int(r[6]==1),'Legacy save did not reject changed affiliation')
    require(len({(r[5],r[17],r[27]) for r in rows.values()})==1,'Generation handles changed in phase')
    if phase==1:
        require(x['rf_scene_setup_result']==[1,START,48,0],'Isolated setup did not use ordinary Delay')
        require(all(rows[f][6]==1 and rows[f][32]==0 for f in (0,20,40)), 'OFF mutated the original neutral APC')
        require(all(rows[f][6]==0 and rows[f][32:35]==[1,0,1] for f in (58,60,78,100,118,119)), 'Original ON did not change only passive APC26')
        require(state[9]==1 and state[3]==0 and payload and state[4]==len(payload),'Ordinary source save failed')
        sections=checkpoint_sections(payload);data=sections[11]
        require(data[:4]==b'RFAL' and struct.unpack_from('<I',data,4)[0]==1,'Missing RFAL1')
        inner,count=struct.unpack_from('<2I',data,8)
        require(count==1 and len(data)==16+inner+8 and struct.unpack_from('<2I',data,16+inner)==(26,0),'RFAL saved the wrong owner/value')
        inner_data=data[16:16+inner]
        require(inner_data[:4]==b'RFPV' and struct.unpack_from('<I',inner_data,4)[0]==2,'Inner RFPV2 changed')
        saved=dict(bytes=len(payload),sha256=sha(payload),allegiance=[26,0],
            owner_words=rows[119][4:32],pending=rows[119][56],formats=['RFWC','RFAL1','RFPV2','RFVA2','RFVC2'])
        # Event2 starts at52; remaining+4=56. Its delayed repeat must be pending.
        require(saved['pending'] not in (0,NONE),'Original9636 did not remain pending at save')
    else:
        require(previous and state[8]==1 and state[0]==0 and state[1]==previous['bytes'] and
            not state[9] and not any(x['rf_scene_setup_result']) and not any(x['rf_scene_world_load_reject']), 'Fresh ordinary load failed/replayed setup')
        require(rows[0][4:32]==previous['owner_words'] and rows[0][32]==0,'Frame0 lost saved owner/team state or replayed event')
        require(all(r[6]==0 for r in rows.values()) and rows[119][32:35]==[1,0,1] and
            rows[119][36:39]==[0,0,26] and rows[119][56]==NONE, 'Pending original9636 did not repeat value0 after load')
        saved=None
    return dict(result='PASS',saved=saved,observations={str(f):dict(passive_team=r[6],selected_team=r[18],
        calls=r[32:40],event9636_remaining=r[56],handles=[r[5],r[17],r[27]],clock=r[100]) for f,r in rows.items()})


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--prepare-only',type=Path);p.add_argument('--run-root',type=Path);p.add_argument('--seconds',type=int,default=600);a=p.parse_args()
    folder=a.prepare_only or a.run_root or ROOT/'artifacts/xemu'/('vehicle-allegiance-events-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder=folder.resolve()
    folder.mkdir(parents=True,exist_ok=a.prepare_only is not None);level,recipe=prepare_level(folder/'level')
    if a.prepare_only:print(json.dumps(dict(result='PREPARED_NOT_RUN',recipe=recipe),indent=2));return
    require_no_project_xemu(ROOT);hdd=prepare_hdd(ROOT,ROOT/'local/xemu-harness/pacing-base.qcow2')
    names=set(FLAGS)|{p.name for p in DISC.glob('campaign-*') if p.is_file()}|{'scene-preview.flag','player-control.flag','scene-fixture.vpp',CONFIG}
    original={n:(DISC/n).read_bytes() if (DISC/n).exists() else None for n in sorted(names)}
    report=dict(result='FAIL',recipe=recipe,phases={},validation={},binaries={},hdd=str(hdd),limitations=__doc__)
    restore=folder/'input-restore';restore.mkdir()
    for n,b in original.items():
        if b is not None:(restore/n).write_bytes(b)
    report['original_inputs']={n:sha(b) if b is not None else None for n,b in original.items()}
    def save():(folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    try:
        previous=None
        for phase,label in ((1,'source'),(2,'loaded')):
            for n in names:(DISC/n).unlink(missing_ok=True)
            for n,b in inputs(level,phase).items():require(n in names,'Untracked disc input');(DISC/n).write_bytes(b)
            save();build(folder,label);report['binaries'][label]=preserve_launch_binaries(folder/(label+'-binaries'));save()
            guest=run_guest(folder,label,hdd,120,a.seconds,snapshot=phase==2,capture_world=phase==1,extra_symbols=SYMBOLS,allow_guest_error=True)
            report['phases'][label]=guest;save()
            payload=(folder/label/'xbox-world.rfwc').read_bytes() if phase==1 else None
            report['validation'][label]=validate(guest,phase,payload,previous);previous=report['validation'][label]['saved'];save()
        report['result']='PASS'
    except BaseException as e:report['error']=type(e).__name__+': '+str(e);raise
    finally:
        for n,b in original.items():
            (DISC/n).unlink(missing_ok=True)
            if b is not None:(DISC/n).write_bytes(b)
        # Restore the image from restored inputs. This never touches another checkout.
        try:build(folder,'restore')
        finally:
            report['restored_inputs']={n:sha((DISC/n).read_bytes()) if (DISC/n).exists() else None for n in sorted(names)}
            report['disc_restored']=report['original_inputs']==report['restored_inputs'];save();print(folder,report['result'],flush=True)
        require(report['disc_restored'],'Disc restoration failed')
if __name__=='__main__':main()
