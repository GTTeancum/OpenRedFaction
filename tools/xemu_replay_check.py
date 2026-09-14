"""Replay a bounded command file inside Xbox and compare native guest state to PC."""
import argparse,datetime,hashlib,json,os,re,shutil,socket,struct,subprocess,sys,time
from pathlib import Path
from xemu_smoke import Monitor
from door_fixture_metrics import measure
from xemu_guest_snapshot import words,snapshot
p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('--seconds',type=int,default=180);p.add_argument('--require-wide',action='store_true');p.add_argument('--campaign-spawn',action='store_true');p.add_argument('--approach',action='store_true',help='Stage outside the first L1S2 climb region');p.add_argument('--climb',action='store_true',help='Staged L1S2 first-region campaign replay');p.add_argument('--capture',action='store_true',help='Native guest framebuffer for renderer validation');p.add_argument('--door',action='store_true',help='Explicit L1S1 lower-door contact fixture');p.add_argument('--level',help='Authored campaign level, without climb staging');p.add_argument('--archive',default='levels1.vpp',choices=['levels1.vpp','levels2.vpp','levels3.vpp','levelsm.vpp']);p.add_argument('--audio-capture',action='store_true',help='Enable APU events and inspect guest DSP output');p.add_argument('--lift',action='store_true',help='Staged L1S2 lift contact');p.add_argument('--force-uid',type=int,help='Explicit authored force-region staging; requires --level');p.add_argument('--damage-uid',type=int,help='Explicit NPC damage plus player pain-sound fixture');p.add_argument('--death-animation',action='store_true',help='Exercise base NPC death animation at frame 120; requires --damage-uid');p.add_argument('--actor-pairs',action='store_true',help='Restored-state registered actor response publication fixture');p.add_argument('--glare-loss',action='store_true',help='Destroy three glares at frame90 through parent-loss/marked retirement');p.add_argument('--volume-test',action='store_true',help='Copied-owner animated beam through the live sorted queue');p.add_argument('--actor-uid',type=int,help='Process-local camera facing an authored actor; requires --level');p.add_argument('--lightmap-regen',action='store_true',help='Force initial world lightmap base regeneration');p.add_argument('--lightmap-all',action='store_true',help='Full-level regeneration stress test, bypassing visibility');p.add_argument('--item-uid',type=int,help='Process-local staging near an authored pickup; requires --level');p.add_argument('--exit-uid',type=int,help='Dispatch authored level exit at frame60');p.add_argument('--exit-start-uid',type=int,help='Walk from outside a linked authored exit volume');p.add_argument('--goal-uid',type=int,help='Dispatch authored goal setter at frame30');p.add_argument('--return-exit-uid',type=int,help='Return exit at frame180 and restage the original item');p.add_argument('--watch-uid',type=int,help='Damage two linked watcher NPCs at frames30/60');p.add_argument('--goto-uid',type=int,help='Dispatch authored Goto or Attack event at30');p.add_argument('--setup-uid',type=int,nargs='+',help='One or two authored Delay/Remove_Object/Slay_Object/Message/Make_Invulnerable/Set_Friendliness/Heal/Armor/Give_Item_To_Player events at0/60; optional Goto at300/360');p.add_argument('--follow-uid',type=int,help='Process-local tracking camera; requires --level');args=p.parse_args()
if args.follow_uid is not None and (not args.level or not 0<args.follow_uid<0xffffffff):p.error('--follow-uid requires --level and a positive UID')
if args.setup_uid is not None and (not args.level or len(args.setup_uid)>2 or any(not 0<x<0xffffffff for x in args.setup_uid)):p.error('--setup-uid requires --level and positive UIDs')
if args.goto_uid is not None and (not args.level or not 0<args.goto_uid<0xffffffff):p.error('--goto-uid requires --level and a positive UID')
if args.watch_uid is not None and (not args.level or not 0<args.watch_uid<0xffffffff):p.error('--watch-uid requires --level and a positive UID')
if args.return_exit_uid is not None and (args.exit_uid is None or not 0<args.return_exit_uid<0xffffffff):p.error('--return-exit-uid requires --exit-uid')
if args.goal_uid is not None and (not args.level or not 0<args.goal_uid<0xffffffff):p.error('--goal-uid requires --level and a positive UID')
if args.exit_start_uid is not None and (not args.level or args.exit_uid is not None or args.item_uid is not None or args.actor_uid is not None or args.force_uid is not None or args.door or args.lift or args.climb or not 0<args.exit_start_uid<0xffffffff):p.error('--exit-start-uid requires --level without another staging/forced exit')
if args.exit_uid is not None and (not args.level or not 0<args.exit_uid<0xffffffff):p.error('--exit-uid requires --level and a positive UID')
if args.lightmap_all and not args.lightmap_regen:p.error('--lightmap-all requires --lightmap-regen')
if args.item_uid is not None and (not args.level or args.climb or args.approach or args.door or args.lift or args.force_uid is not None or args.actor_uid is not None or not 0<=args.item_uid<0xffffffff):p.error('--item-uid requires --level and no other staging')
if args.actor_uid is not None and (not args.level or args.climb or args.approach or args.door or args.lift or args.force_uid is not None or not 0<=args.actor_uid<0xffffffff):p.error('--actor-uid requires --level and no other staging')
if args.death_animation and args.damage_uid is None:p.error('--death-animation requires --damage-uid')
if args.damage_uid is not None and not 0<=args.damage_uid<0xffffffff:p.error('--damage-uid requires an unsigned actor UID')
if args.force_uid is not None and (not args.level or args.climb or args.door or args.lift or not 0<=args.force_uid<=0xffffffff):p.error('--force-uid requires --level and no other staging')
if args.lift:
 if args.door or args.climb or args.approach or args.level:p.error('--lift requires its own L1S2 fixture')
 args.level='L1S2.rfl';args.campaign_spawn=True
if args.damage_uid is not None:args.campaign_spawn=True
if args.door:
 if args.climb or args.approach or args.level:p.error('--door cannot combine with level/climb staging')
 args.campaign_spawn=True
if args.approach:args.climb=True
if args.climb:args.campaign_spawn=True
if args.level:
 if args.climb or not re.fullmatch(r'[A-Za-z0-9_-]+\.rfl',args.level) or len(args.level)>63:p.error('Choose a plain level name, without climb staging')
 args.campaign_spawn=True
elif args.archive!='levels1.vpp':p.error('--archive requires --level')
replay_env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
if args.return_exit_uid is not None:replay_env['RF_REPLAY_RETURN_EXIT_UID']=str(args.return_exit_uid)
if args.follow_uid is not None:replay_env['RF_REPLAY_FOLLOW_UID']=str(args.follow_uid)
if args.setup_uid is not None:replay_env['RF_REPLAY_SETUP_UID']=','.join(map(str,args.setup_uid))
if args.goto_uid is not None:replay_env['RF_REPLAY_GOTO_UID']=str(args.goto_uid)
if args.watch_uid is not None:replay_env['RF_REPLAY_WATCH_UID']=str(args.watch_uid)
if args.goal_uid is not None:replay_env['RF_REPLAY_GOAL_UID']=str(args.goal_uid)
if args.exit_uid is not None:replay_env['RF_REPLAY_EXIT_UID']=str(args.exit_uid)
if args.exit_start_uid is not None:replay_env['RF_REPLAY_EXIT_START']=str(args.exit_start_uid)
for key in ('RF_REPLAY_ITEM_UID','RF_REPLAY_ACTOR_UID','RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START','RF_REPLAY_DOOR_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID','RF_REPLAY_DAMAGE_UID','RF_REPLAY_DEATH_ANIMATION'):replay_env.pop(key,None)
replay_env.update(RF_REPLAY_LEVEL=args.level or ('L1S2.rfl' if args.climb else 'L1S1.rfl'),RF_REPLAY_ARCHIVE=args.archive)
if args.damage_uid is not None:replay_env['RF_REPLAY_DAMAGE_UID']=str(args.damage_uid)
if args.death_animation:replay_env['RF_REPLAY_DEATH_ANIMATION']='1'
replay_env.pop('RF_REPLAY_VOLUME_TEST',None)
if args.volume_test:replay_env['RF_REPLAY_VOLUME_TEST']='1'
replay_env.pop('RF_REPLAY_LIGHTMAP_REGEN',None)
if args.lightmap_regen:replay_env['RF_REPLAY_LIGHTMAP_REGEN']='2' if args.lightmap_all else '1'
replay_env.pop('RF_REPLAY_GLARE_LOSS',None)
if args.glare_loss:replay_env['RF_REPLAY_GLARE_LOSS']='1'
replay_env.pop('RF_REPLAY_ACTOR_PAIRS',None)
if args.actor_pairs:replay_env['RF_REPLAY_ACTOR_PAIRS']='1'
if args.actor_uid is not None:replay_env['RF_REPLAY_ACTOR_UID']=str(args.actor_uid)
if args.item_uid is not None:replay_env['RF_REPLAY_ITEM_UID']=str(args.item_uid)
if args.force_uid is not None:replay_env['RF_REPLAY_FORCE_UID']=str(args.force_uid)
if args.lift:replay_env['RF_REPLAY_LIFT_START']='1'
if args.door:replay_env['RF_REPLAY_DOOR_START']='1'
if args.climb:replay_env['RF_REPLAY_REGION_START']='2' if args.approach else '1'

root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu');payload=args.input.read_bytes()
record_size={b'RFI2':28,b'RFI3':32,b'RFI4':40,b'RFI5':44}.get(payload[:4],24)
offset=8 if record_size!=24 else 0
if offset and payload[4:8]!=record_size.to_bytes(4,'little'):raise ValueError('Invalid replay record size')
if len(payload)<=offset or (len(payload)-offset)%record_size or (len(payload)-offset)>60000*record_size:raise ValueError('Expected 1..60000 input records')
frames=(len(payload)-offset)//record_size
if args.volume_test and (frames<=90 or not args.level):p.error('--volume-test requires --level and at least91 frames')
if args.glare_loss and frames<=91:p.error('--glare-loss requires at least92 frames')
if args.death_animation and frames<=120:p.error('--death-animation requires at least 121 replay frames')
run=root/'artifacts/xemu'/('replay-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
source=run/'inputs.bin';source.write_bytes(payload)
pc=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay' if args.campaign_spawn else '--replay',str(root/'Installed_Game'),str(source),str(run/'pc-final.ppm')],capture_output=True,text=True,check=True,env=replay_env)
(run/'pc-reference.txt').write_text(pc.stdout)
pc_transitions=[line.split()[1:] for line in pc.stdout.splitlines() if line.startswith('LEVEL_TRANSITION ')]
final_level=pc_transitions[-1][1] if pc_transitions else replay_env['RF_REPLAY_LEVEL']
section_frames=frames-int(pc_transitions[-1][3]) if pc_transitions else frames
def expected(label):return list(map(int,next(x for x in pc.stdout.splitlines() if x.startswith(label+' ')).split()[1:]))
if args.campaign_spawn and not args.climb and not args.door and not args.lift and args.force_uid is None and args.actor_uid is None and args.item_uid is None and args.exit_start_uid is None:
 starts=json.loads((root/'artifacts/player-start-verification.json').read_text())
 look=json.loads((root/'artifacts/player-spawn-look.json').read_text())
 original='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
 assert starts['original_sha256']==look['original_sha256']==original
 start=next(c for c in starts['levels'] if c['file'].lower()==final_level.lower())
 angles=next(c for c in look['cases'] if c['file'].lower()==final_level.lower())
 assert expected('PLAYER_SPAWN')==[1]+start['transform_words']+angles['body_words']+angles['eye_words']
if args.campaign_spawn:assert expected('PC_PLAY_BODY')[68]&0x80, 'Campaign player physics flag missing'
hdd=root/'local/xemu-harness/pacing-base.qcow2'
if not hdd.exists():raise ValueError('Run the pacing harness once to prepare its separate HDD base')
assert (root/'build/xbox/disc/player-control.flag').exists()
replay=root/'build/xbox/disc/player-replay.bin';saved=replay.read_bytes() if replay.exists() else None
spawn_flag=root/'build/xbox/disc/campaign-spawn.flag';saved_spawn=spawn_flag.read_bytes() if spawn_flag.exists() else None
audio_flag=root/'build/xbox/disc/audio-output.flag';saved_audio=audio_flag.read_bytes() if audio_flag.exists() else None
lift_flag=root/'build/xbox/disc/campaign-lift.flag';saved_lift=lift_flag.read_bytes() if lift_flag.exists() else None
door_flag=root/'build/xbox/disc/campaign-door.flag';saved_door=door_flag.read_bytes() if door_flag.exists() else None
climb_flag=root/'build/xbox/disc/campaign-climb.flag';saved_climb=climb_flag.read_bytes() if climb_flag.exists() else None
selection_file=root/'build/xbox/disc/campaign-level.bin';saved_selection=selection_file.read_bytes() if selection_file.exists() else None
exit_start_file=root/'build/xbox/disc/campaign-exit-start.bin';saved_exit_start=exit_start_file.read_bytes() if exit_start_file.exists() else None
return_file=root/'build/xbox/disc/campaign-return.bin';saved_return=return_file.read_bytes() if return_file.exists() else None
follow_file=root/'build/xbox/disc/campaign-follow.bin';saved_follow=follow_file.read_bytes() if follow_file.exists() else None
setup_file=root/'build/xbox/disc/campaign-setup.bin';saved_setup=setup_file.read_bytes() if setup_file.exists() else None
goto_file=root/'build/xbox/disc/campaign-goto.bin';saved_goto=goto_file.read_bytes() if goto_file.exists() else None
watch_file=root/'build/xbox/disc/campaign-watch.bin';saved_watch=watch_file.read_bytes() if watch_file.exists() else None
goal_file=root/'build/xbox/disc/campaign-goal.bin';saved_goal=goal_file.read_bytes() if goal_file.exists() else None
exit_file=root/'build/xbox/disc/campaign-exit.bin';saved_exit=exit_file.read_bytes() if exit_file.exists() else None
item_file=root/'build/xbox/disc/campaign-item.bin';saved_item=item_file.read_bytes() if item_file.exists() else None
actor_file=root/'build/xbox/disc/campaign-actor.bin';saved_actor=actor_file.read_bytes() if actor_file.exists() else None
force_file=root/'build/xbox/disc/campaign-force.bin';saved_force=force_file.read_bytes() if force_file.exists() else None
damage_file=root/'build/xbox/disc/campaign-damage.bin';saved_damage=damage_file.read_bytes() if damage_file.exists() else None
death_flag=root/'build/xbox/disc/campaign-death-animation.flag';saved_death=death_flag.read_bytes() if death_flag.exists() else None
pair_flag=root/'build/xbox/disc/campaign-actor-pairs.flag';saved_pair=pair_flag.read_bytes() if pair_flag.exists() else None
volume_flag=root/'build/xbox/disc/campaign-volume-test.flag';saved_volume=volume_flag.read_bytes() if volume_flag.exists() else None
regen_flag=root/'build/xbox/disc/campaign-lightmap-regen.flag';saved_regen=regen_flag.read_bytes() if regen_flag.exists() else None
loss_flag=root/'build/xbox/disc/campaign-glare-loss.flag';saved_loss=loss_flag.read_bytes() if loss_flag.exists() else None
step_file=root/'build/xbox/disc/particle-step-fixtures.bin';saved_steps=step_file.read_bytes() if step_file.exists() else None
process=monitor=None;report={'result':'FAIL','level':args.level or ('L1S2.rfl' if args.climb else 'L1S1.rfl'),'final_level':final_level,'archive':args.archive,'frames':frames,'input_sha256':hashlib.sha256(payload).hexdigest(),'pc_sha256':hashlib.sha256((root/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(),'samples':[],'scope':'Guest command replay, submission counts, CPU world/camera hashes and final body; optional native framebuffer capture, no PS2 parity claim.'}
def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True)
try:
 if args.return_exit_uid is None:return_file.unlink(missing_ok=True)
 else:return_file.write_bytes(struct.pack('<II',args.return_exit_uid,args.item_uid or 0))
 if args.follow_uid is None:follow_file.unlink(missing_ok=True)
 else:follow_file.write_bytes(args.follow_uid.to_bytes(4,'little'))
 if args.setup_uid is None:setup_file.unlink(missing_ok=True)
 else:setup_file.write_bytes(b''.join(x.to_bytes(4,'little') for x in args.setup_uid))
 if args.goto_uid is None:goto_file.unlink(missing_ok=True)
 else:goto_file.write_bytes(args.goto_uid.to_bytes(4,'little'))
 if args.watch_uid is None:watch_file.unlink(missing_ok=True)
 else:watch_file.write_bytes(args.watch_uid.to_bytes(4,'little'))
 if args.goal_uid is None:goal_file.unlink(missing_ok=True)
 else:goal_file.write_bytes(args.goal_uid.to_bytes(4,'little'))
 if args.exit_start_uid is None:exit_start_file.unlink(missing_ok=True)
 else:exit_start_file.write_bytes(args.exit_start_uid.to_bytes(4,'little'))
 if args.exit_uid is None:exit_file.unlink(missing_ok=True)
 else:exit_file.write_bytes(args.exit_uid.to_bytes(4,'little'))
 subprocess.run([sys.executable,'tools/verify_particle_free_step.py'],cwd=root,check=True)
 step_file.write_bytes((root/'artifacts/particle-free-step-input.bin').read_bytes())
 if args.level:
  archive_source=root/'Installed_Game'/args.archive;archive_target=root/'build/xbox/disc'/args.archive
  archive_sha=hashlib.sha256(archive_source.read_bytes()).hexdigest()
  if not archive_target.exists():shutil.copyfile(archive_source,archive_target)
  if hashlib.sha256(archive_target.read_bytes()).hexdigest()!=archive_sha:raise ValueError('Staged archive differs from installed source')
  report['archive_sha256']=archive_sha
 if args.level:selection_file.write_bytes(args.archive.encode().ljust(64,b'\0')+args.level.encode().ljust(64,b'\0'))
 else:selection_file.unlink(missing_ok=True)
 if args.volume_test:volume_flag.write_bytes(b'1')
 else:volume_flag.unlink(missing_ok=True)
 if args.lightmap_regen:regen_flag.write_bytes(b'2' if args.lightmap_all else b'1')
 else:regen_flag.unlink(missing_ok=True)
 if args.glare_loss:loss_flag.write_bytes(b'1')
 else:loss_flag.unlink(missing_ok=True)
 if args.actor_pairs:pair_flag.write_bytes(b'1')
 else:pair_flag.unlink(missing_ok=True)
 if args.death_animation:death_flag.write_bytes(b'1')
 else:death_flag.unlink(missing_ok=True)
 if args.damage_uid is None:damage_file.unlink(missing_ok=True)
 else:damage_file.write_bytes(args.damage_uid.to_bytes(4,'little'))
 if args.item_uid is not None:item_file.write_bytes(args.item_uid.to_bytes(4,'little'))
 else:item_file.unlink(missing_ok=True)
 if args.actor_uid is not None:actor_file.write_bytes(args.actor_uid.to_bytes(4,'little'))
 else:actor_file.unlink(missing_ok=True)
 if args.force_uid is not None:force_file.write_bytes(args.force_uid.to_bytes(4,'little'))
 else:force_file.unlink(missing_ok=True)
 if args.climb:climb_flag.write_bytes(b'2' if args.approach else b'')
 else:climb_flag.unlink(missing_ok=True)
 if args.lift:lift_flag.write_bytes(b'1')
 else:lift_flag.unlink(missing_ok=True)
 if args.door:door_flag.write_bytes(b'1')
 else:door_flag.unlink(missing_ok=True)
 if args.audio_capture:audio_flag.write_bytes(b'1')
 else:audio_flag.unlink(missing_ok=True)
 report['staged_door']=args.door
 replay.write_bytes(payload)
 if args.campaign_spawn:spawn_flag.write_bytes(b'')
 elif spawn_flag.exists():spawn_flag.unlink()
 build();mapping=(root/'build/xbox/main.map').read_text()
 (run/'main.map').write_text(mapping);shutil.copyfile(root/'build/xbox/disc/default.xbe',run/'default.xbe')
 def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
 report.update(map_sha256=hashlib.sha256(mapping.encode()).hexdigest(),xbe_sha256=hashlib.sha256((root/'build/xbox/disc/default.xbe').read_bytes()).hexdigest(),iso_sha256=hashlib.sha256((root/'build/xbox/redfaction-diagnostic.iso').read_bytes()).hexdigest())
 with socket.socket() as reservation:reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
 eeprom=run/'eeprom.bin';shutil.copyfile(emulator/'eeprom.bin',eeprom)
 config=run/'xemu.toml';config.write_text(f'''[general]
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
eeprom_path = '{eeprom.as_posix()}'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{root.as_posix()}/build/xbox/redfaction-diagnostic.iso'
''')
 command=[str(emulator/'xemu.exe'),'-config_path',str(config),'-m','64','-snapshot','-display','xemu','-audio','none','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off'];report['command']=command
 startup=None
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
  process=subprocess.Popen(command,cwd=run,env=dict(os.environ,SDL_AUDIO_DRIVER='dummy'),stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
  deadline=time.monotonic()+args.seconds;last=-1;lighting_poll=0;last_transition=None;fault_poll=0
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError(f'XEMU exited {process.returncode}')
   if monitor is None:
    try:monitor=Monitor(port)
    except OSError:time.sleep(.5);continue
    report['memory']=monitor.command('query-memory-size-summary');assert report['memory']['base-memory']==64*1024*1024
   try:d=words(monitor,symbol('rf_diagnostic'),58)
   except RuntimeError as exc:
    if 'received 0' not in str(exc):raise
    time.sleep(.5);continue
   if d[0]!=0x52464447:time.sleep(.5);continue
   report['samples'].append(d)
   if (args.exit_uid is not None or args.exit_start_uid is not None) and time.monotonic()>=fault_poll:
    fault_poll=time.monotonic()+30
    regs=monitor.command('human-monitor-command',{'command-line':'info registers'})
    # Specific debug BIOS bugcheck halt observed in both failed handoff runs.
    if 'EIP=8001d1ea' in regs and 'HLT=1' in regs:raise RuntimeError('Guest debug-kernel bugcheck halt at 8001d1ea')

   if args.lightmap_regen and time.monotonic()>=lighting_poll:
    lighting_poll=time.monotonic()+10
    progress=words(monitor,symbol('rf_scene_lightmap_regeneration'),8)
    report.setdefault('lighting_progress',[]).append({'remaining_seconds':round(deadline-time.monotonic(),1),'state':progress})
    print('Lighting jobs/callbacks/stage:',progress[0],progress[2],progress[7],flush=True)
    (run/'lighting-progress.json').write_text(json.dumps(report['lighting_progress'],indent=2))
   if args.audio_capture:
    report['device_audio']=words(monitor,symbol('rf_xbox_audio_diagnostic'),12)
    report['audio_close_phase']=words(monitor,symbol('rf_xbox_audio_close_phase'),1)
   if d[2]&0x80000000:
    report['lightmap_regeneration_failure']=words(monitor,symbol('rf_scene_lightmap_regeneration'),8)
    report['lightmap_updates_failure']=words(monitor,symbol('rf_scene_lightmap_updates'),8)
    raise RuntimeError(f'Guest error {d[2]:08x}')
   if d[37]//60!=last:last=d[37]//60;print('Submitted',d[37],'frames',flush=True)
   if args.exit_uid is not None or args.exit_start_uid is not None:
    transition=words(monitor,symbol('rf_xbox_level_transitions'),4)+words(monitor,symbol('rf_xbox_load_stage'),1)
    if transition!=last_transition:
     print('Level handoff',transition,flush=True);report.setdefault('handoffs',[]).append({'state':transition[:4],'load_stage':transition[4]});last_transition=transition
   if d[2]==5:
    final=snapshot(monitor,mapping);(run/'guest-memory-final.json').write_text(json.dumps(final,indent=2))
    fp_control=words(monitor,symbol('rf_fp_control_diagnostic'),5)
    report['x87_control_words']=fp_control
    assert fp_control==[0x27f]*5,fp_control
    step_state=words(monitor,symbol('rf_particle_step_diagnostic'),4)
    step_expected=(root/'artifacts/particle-free-step-output.bin').read_bytes();step_hash=2166136261
    for value in step_expected:step_hash=((step_hash^value)*16777619)&0xffffffff
    assert step_state==[1,len(step_expected)//128,step_hash,192000],step_state
    report['particle_steps']=step_state
    burn_retarget=words(monitor,symbol('rf_burn_retarget_diagnostic'),8)
    burn_expected=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_burn_retarget_tests.exe')],text=True).split()))
    assert burn_retarget==burn_expected and burn_retarget[7]==1,burn_retarget
    report['burn_retarget_resources']=burn_retarget
    print('Native x87 control:',[hex(v) for v in fp_control],flush=True)
    resources=words(monitor,symbol('rf_particle_resource_diagnostic'),7)
    assert resources[0:2]==[1,6] and resources[4]>0 and resources[5]<resources[4] and resources[6]>=resources[4],resources
    expected_resource_hash=2166136261
    for repeat in range(2):
     for resource,frame in [('LightCorona01.tga',None),('boom01.vbm',0),('boom01.vbm',15)]:
      raw=run/'particle-resource.rgba'
      command=[str(root/'build/pc/Release/rf_image_probe.exe'),str(root/'Installed_Game/maps2.vpp'),resource,str(raw),'65536']
      if frame is not None:command.append(str(frame))
      subprocess.run(command,check=True,capture_output=True)
      for value in raw.read_bytes():expected_resource_hash=((expected_resource_hash^value)*16777619)&0xffffffff
    assert resources[2]==expected_resource_hash,resources
    report['particle_resources']=resources
    loading=words(monitor,symbol('rf_explosion_loading_diagnostic'),6)
    assert loading[0:2]==[1,9],loading
    definition_hash=2166136261
    for recipe in ('generic','space','geomod','shoulder mounted geomod','rocket hit','flamethrower-alt','FGatE','FGatE_lilspark','FGatE_bigspark'):
     data=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--explosion-definition',str(root/'Installed_Game/tables.vpp'),recipe,'65536'])
     assert data[:4]==bytes(4) and len(data)==2384
     for value in data[4:]:definition_hash=((definition_hash^value)*16777619)&0xffffffff
    vclip=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--vclip-load',str(root/'Installed_Game/tables.vpp'),'charge_explode','65536'])
    assert vclip[:4]==bytes(4) and len(vclip)==504
    vclip_hash=2166136261
    for value in vclip[4:]:vclip_hash=((vclip_hash^value)*16777619)&0xffffffff
    rocket=subprocess.check_output([str(root/'build/pc/Release/rf_effect_probe.exe'),'--explosion-definition',str(root/'Installed_Game/tables.vpp'),'rocket hit','65536'])
    assert rocket[:4]==bytes(4) and len(rocket)==2384
    assert loading[2:]==[definition_hash,43990,vclip_hash,int.from_bytes(rocket[2372:2376],'little')],loading
    report['explosion_loading']=loading


    for name,label,count in [('rf_scene_actor_follow_summary','ACTOR_FOLLOW_SUMMARY',5),('rf_scene_player_input_frames','ACTOR_PLAYER_INPUT',448),('scene_actor_body','PC_PLAY_BODY',77)]:
     got=words(monitor,symbol(name),count);assert got==expected(label),name;report[name]=got
    if args.campaign_spawn:
     report['player_life']=words(monitor,symbol('rf_scene_player_life'),8)
     assert report['player_life']==expected('PLAYER_LIFE'),report['player_life']
     report['enemy_awareness']=words(monitor,symbol('rf_scene_enemy_awareness'),8)
     assert report['enemy_awareness']==expected('ENEMY_AWARENESS') and report['enemy_awareness'][7]==0,report['enemy_awareness']
     report['pickup_vitals']=words(monitor,symbol('rf_scene_pickup_vitals'),4)
     assert report['pickup_vitals']==expected('PICKUP_VITALS'),report['pickup_vitals']
     report['pickups']=words(monitor,symbol('rf_scene_pickups'),8)
     assert report['pickups']==expected('PICKUPS') and report['pickups'][7]==0,report['pickups']
     report['weapon_selection']=words(monitor,symbol('rf_scene_weapon_selection'),8)
     assert report['weapon_selection']==expected('WEAPON_SELECTION'),report['weapon_selection']
     report['player_ammo']=words(monitor,symbol('rf_scene_player_ammo'),8)
     assert report['player_ammo']==expected('PLAYER_AMMO') and report['player_ammo'][7]==0,report['player_ammo']
     report['pistol_rules']=words(monitor,symbol('rf_scene_pistol_rules'),7)
     assert report['pistol_rules']==expected('PISTOL_RULES'),report['pistol_rules']
     report['player_weapon']=words(monitor,symbol('rf_scene_player_weapon'),8)
     assert report['player_weapon']==expected('PLAYER_WEAPON') and report['player_weapon'][6]==0,report['player_weapon']
     report['weapon_audio']=words(monitor,symbol('rf_scene_weapon_audio'),9)
     assert report['weapon_audio']==expected('WEAPON_AUDIO') and report['weapon_audio'][7]==0,report['weapon_audio']
     report['combat_death']=words(monitor,symbol('rf_scene_combat_death'),8)
     assert report['combat_death']==expected('COMBAT_DEATH') and report['combat_death'][3]==0,report['combat_death']
     report['enemy_combat']=words(monitor,symbol('rf_scene_enemy_combat'),8)
     assert report['enemy_combat']==expected('ENEMY_COMBAT') and report['enemy_combat'][7]==0,report['enemy_combat']
     report['enemy_actions']={}
     for label,sym,count in [('SCRIPT_ATTACK','rf_scene_script_attack',12),('ENEMY_AIM','rf_scene_enemy_aim',4),('ENEMY_FIRE','rf_scene_enemy_fire',6),('ENEMY_RETALIATION','rf_scene_enemy_retaliation',4)]:
      actual=words(monitor,symbol(sym),count)
      report['enemy_actions'][label]=actual
      assert actual==expected(label),(label,actual,expected(label))
     report['combat']=words(monitor,symbol('rf_scene_combat'),8)
     assert report['combat']==expected('COMBAT') and report['combat'][7]==0,report['combat']
     forces=words(monitor,symbol('rf_scene_campaign_forces'),3)
     assert forces==expected('CAMPAIGN_FORCES') and forces[1]<=65536,forces
     report['campaign_forces']=forces
     force_state=words(monitor,symbol('rf_scene_force_state'),3)
     assert force_state==expected('FORCE_STATE'),force_state
     report['force_state']=force_state
     force_ticks=words(monitor,symbol('rf_scene_force_ticks'),12)
     assert force_ticks==expected('FORCE_TICKS'),force_ticks
     report['force_ticks']=force_ticks
     owned=words(monitor,symbol('rf_scene_campaign_events'),3)
     assert owned[0]==expected('CAMPAIGN_EVENTS')[0] and owned[1]<=1024*1024
     report['campaign_events']=owned
     switches=words(monitor,symbol('rf_scene_switch_state'),3)
     assert switches==expected('SWITCH_STATE'),switches
     report['switch_state']=switches
     lights=words(monitor,symbol('rf_scene_light_owner'),8)
     report['light_fields']=words(monitor,symbol('rf_scene_light_fields'),34)
     report['light_fields_pc']=expected('LIGHT_FIELDS')
     assert report['light_fields']==report['light_fields_pc'],report['light_fields']
     assert lights==expected('LIGHT_OWNER') and lights[1]<=256*1024 and lights[0]==lights[2],lights
     report['light_owner']=lights
     report['lightmap_regeneration']=words(monitor,symbol('rf_scene_lightmap_regeneration'),8)
     assert report['lightmap_regeneration']==expected('LIGHTMAP_REGEN') and report['lightmap_regeneration'][6]==0,report['lightmap_regeneration']
     if args.lightmap_regen:assert report['lightmap_regeneration'][0]>0 and report['lightmap_regeneration'][2]>0 and report['lightmap_regeneration'][5]==0,report['lightmap_regeneration']
     report['lightmap_updates']=words(monitor,symbol('rf_scene_lightmap_updates'),8)
     pc_updates=expected('LIGHTMAP_UPDATES')
     # Lighting updates execute every tick on both paths, even when PC rasterization is skipped.
     assert report['lightmap_updates']==pc_updates and report['lightmap_updates'][7]==0,report['lightmap_updates']
     report['light_ticks']=words(monitor,symbol('rf_scene_light_ticks'),8)
     assert report['light_ticks']==expected('LIGHT_TICKS') and report['light_ticks'][6]<=64*1024 and report['light_ticks'][7]==0,report['light_ticks']
     if lights[0]:assert report['light_ticks'][:2]==[section_frames-1,(section_frames-1)*lights[0]],report['light_ticks']
     report['light_storage']=words(monitor,symbol('rf_scene_light_storage'),5)
     assert report['light_storage']==expected('LIGHT_STORAGE') and report['light_storage'][2]<=512*1024,report['light_storage']
     ambient=words(monitor,symbol('rf_scene_ambient_records'),3)
     assert ambient==expected('AMBIENT_RECORDS') and ambient[1]<=65536,ambient
     report['ambient_records']=ambient
     ambient_instances=words(monitor,symbol('rf_scene_ambient_instances'),4)
     assert ambient_instances==expected('AMBIENT_INSTANCES') and ambient_instances[2]<=65536,ambient_instances
     assert ambient_instances[0]+ambient_instances[1]==ambient[0],ambient_instances
     report['ambient_instances']=ambient_instances
     metadata=words(monitor,symbol('rf_scene_sound_metadata'),8)
     assert metadata==expected('SOUND_METADATA') and metadata[:3]==[2712,355344,263],metadata
     report['sound_metadata']=metadata
     ambient_audio=words(monitor,symbol('rf_scene_ambient_audio'),8)
     assert ambient_audio==expected('AMBIENT_AUDIO'),ambient_audio
     report['ambient_audio']=ambient_audio
     ambient_schedule=words(monitor,symbol('rf_scene_ambient_schedule'),6)
     assert ambient_schedule==expected('AMBIENT_SCHEDULE') and ambient_schedule[2]<=25,ambient_schedule
     report['ambient_schedule']=ambient_schedule
     triggers=words(monitor,symbol('rf_scene_campaign_triggers'),2)
     assert triggers[0]==expected('CAMPAIGN_TRIGGERS')[0] and triggers[1]<=1024*1024
     report['campaign_triggers']=triggers
     memberships=words(monitor,symbol('rf_scene_campaign_memberships'),5)
     assert memberships==expected('CAMPAIGN_MEMBERSHIPS'),memberships
     report['campaign_memberships']=memberships
     movers=words(monitor,symbol('rf_scene_campaign_movers'),3)
     assert movers==expected('CAMPAIGN_MOVERS'),movers
     report['campaign_movers']=movers
     startup=words(monitor,symbol('rf_scene_npc_startup'),4)
     assert startup==expected('NPC_STARTUP') and startup[0]>0,startup
     report['npc_startup']=startup
     npc_geometry=words(monitor,symbol('rf_scene_npc_geometry'),7)
     assert npc_geometry==expected('NPC_GEOMETRY') and npc_geometry[4]<=1024*1024,npc_geometry
     report['npc_geometry']=npc_geometry
     npc_materials=words(monitor,symbol('rf_scene_npc_materials'),8)
     assert npc_materials==expected('NPC_MATERIALS') and npc_materials[4]<=5*1024*1024,npc_materials
     assert npc_materials[0]>0 and npc_materials[2]>0 and npc_materials[6]>0,npc_materials
     report['npc_materials']=npc_materials
     npc_draw=words(monitor,symbol('rf_scene_npc_draw'),5)
     assert npc_draw==expected('NPC_DRAW') and npc_draw[0]==section_frames and npc_draw[4]<512*1024,npc_draw
     report['npc_draw']=npc_draw
     npc_playback=words(monitor,symbol('rf_scene_npc_playback'),7)
     assert npc_playback==expected('NPC_PLAYBACK') and npc_playback[0]==section_frames-1 and npc_playback[6]<=1024*1024,npc_playback
     if section_frames>1:assert npc_playback[1:3]==startup[:2],npc_playback
     report['npc_playback']=npc_playback
     npc_gate=words(monitor,symbol('rf_scene_npc_gate'),4)
     assert npc_gate==expected('NPC_GATE') and npc_gate[0]==(section_frames-1)*startup[0],npc_gate
     assert npc_gate[1]+npc_gate[2]==npc_gate[0],npc_gate
     report['npc_gate']=npc_gate
     npc_bodies=words(monitor,symbol('rf_scene_npc_bodies'),6)
     assert npc_bodies==expected('NPC_BODIES') and npc_bodies[1]==startup[0],npc_bodies
     watches=words(monitor,symbol('rf_scene_death_watches'),97)
     assert watches[0]<=32
     native_watches=['DEATH_WATCH '+' '.join(map(str,watches[1+i*3:4+i*3])) for i in range(watches[0])]
     assert native_watches==[x for x in pc.stdout.splitlines() if x.startswith('DEATH_WATCH ')],native_watches
     report['death_watches']=native_watches
     watch_test=words(monitor,symbol('rf_scene_watch_test'),4)
     assert watch_test==expected('WATCH_TEST'),watch_test
     report['watch_test']=watch_test
     npc_triggers=words(monitor,symbol('rf_scene_npc_triggers'),6)
     assert npc_triggers==expected('NPC_TRIGGERS'),npc_triggers
     report['npc_triggers']=npc_triggers
     slays=words(monitor,symbol('rf_scene_script_slays'),6)
     assert slays==expected('SCRIPT_SLAYS'),slays
     report['script_slays']=slays
     message_audio=words(monitor,symbol('rf_scene_message_audio'),4)
     assert message_audio==expected('MESSAGE_AUDIO'),message_audio
     report['message_audio']=message_audio
     for label,sym in [('SCRIPT_MOVE','rf_scene_script_movement'),('SCRIPT_ACTOR','rf_scene_script_actor'),('SCRIPT_ROUTES','rf_scene_script_routes')]:
      actual=words(monitor,symbol(sym),8);assert actual==expected(label),(label,actual,expected(label))
      report[label.lower()]=actual
     retirement=words(monitor,symbol('rf_scene_actor_retirement'),4)
     assert retirement==expected('ACTOR_RETIREMENT') and retirement[1]<=npc_bodies[1] and retirement[3]==0,retirement
     active_npc_count=npc_bodies[1]-retirement[1]
     assert 0<npc_bodies[3]<=npc_bodies[4]<=640*1024,npc_bodies
     report['npc_bodies']=npc_bodies
     navigation=words(monitor,symbol('rf_scene_navigation'),6)
     assert navigation==expected('NAVIGATION') and navigation[4]<=65536,navigation
     if final_level.lower()=='l1s1.rfl':
      # Independently derived from original463d50-audited L1S1 records.
      assert navigation==[333,760,3,1,48360,916127865],navigation
     report['navigation']=navigation
     navigation_workspace=words(monitor,symbol('rf_scene_navigation_workspace'),4)
     assert navigation_workspace==expected('NAVIGATION_WORKSPACE'),navigation_workspace
     if navigation[4]:
      assert navigation_workspace[:3]==[navigation[0],24+(navigation[0]+2)*32+(navigation[1]+2*navigation[0]+2)*4,navigation[1]],navigation_workspace
      assert navigation_workspace[1]<=32768,navigation_workspace
     else:assert navigation_workspace==[0,0,0,0],navigation_workspace
     report['navigation_workspace']=navigation_workspace
     npc_route_test=words(monitor,symbol('rf_scene_npc_route_test'),6)
     assert npc_route_test==expected('NPC_ROUTE_TEST'),npc_route_test
     if args.actor_pairs:
      assert npc_route_test[:3]==[startup[0]]*3 and npc_route_test[4]==npc_bodies[0]*160 and npc_route_test[5]==0,npc_route_test
     report['npc_route_test']=npc_route_test
     npc_look_test=words(monitor,symbol('rf_scene_npc_look_test'),6)
     assert npc_look_test==expected('NPC_LOOK_TEST'),npc_look_test
     if args.actor_pairs:
      assert npc_look_test[:3]==[startup[0]]*3 and npc_look_test[4]==npc_bodies[0]*136 and npc_look_test[5]==0,npc_look_test
     report['npc_look_test']=npc_look_test
     npc_drive_test=words(monitor,symbol('rf_scene_npc_drive_test'),4)
     assert npc_drive_test==expected('NPC_DRIVE_TEST'),npc_drive_test
     if args.actor_pairs:assert npc_drive_test[:2]==[startup[0]]*2 and npc_drive_test[3]==0,npc_drive_test
     report['npc_drive_test']=npc_drive_test
     clutter=words(monitor,symbol('rf_scene_clutter'),8)
     assert clutter==expected('CLUTTER') and clutter[0]==431 and clutter[3]==55610,clutter
     assert clutter[5]<=clutter[6]<=256*1024,clutter
     if final_level.lower()=='l1s1.rfl':
      assert clutter[1]==170 and clutter[4]==39834,clutter
     report['clutter']=clutter
     clutter_render=words(monitor,symbol('rf_scene_clutter_render'),8)
     assert clutter_render==expected('CLUTTER_RENDER'),clutter_render
     assert clutter_render[1]+clutter_render[2]+clutter_render[3]==clutter[1],clutter_render
     assert clutter_render[4]<=clutter_render[5]<=1024*1024,clutter_render
     if final_level.lower()=='l1s1.rfl':
      tag_reference=json.loads((root/'artifacts/clutter-scene-tags.json').read_text())
      assert tag_reference['result']=='PASS' and clutter_render[:4]==[12,168,2,0] and clutter_render[4:6]==tag_reference['render_budget'],clutter_render
     report['clutter_render']=clutter_render
     clutter_tags=words(monitor,symbol('rf_scene_clutter_tags'),4)
     assert clutter_tags==expected('CLUTTER_TAGS') and clutter_tags[0]==clutter_render[0],clutter_tags
     if final_level.lower()=='l1s1.rfl':assert clutter_tags==tag_reference['expected'],clutter_tags
     report['clutter_tags']=clutter_tags
     glare_resources=words(monitor,symbol('rf_scene_glare_resources'),9)
     glare_reference=json.loads((root/'artifacts/glare-scene-resources.json').read_text())
     assert glare_reference['result']=='PASS' and glare_resources==expected('GLARE_RESOURCES')==glare_reference['expected'],glare_resources
     assert glare_resources[3]<=glare_resources[4]<=2*1024*1024,glare_resources
     report['glare_resources']=glare_resources
     geometry_textures=words(monitor,symbol('rf_scene_geometry_textures'),13)
     assert geometry_textures==expected('GEOMETRY_TEXTURES'),geometry_textures
     assert geometry_textures[3]<=128*1024 and geometry_textures[9]==0 and geometry_textures[12]==1,geometry_textures
     if args.actor_pairs:assert geometry_textures[6]>0 and geometry_textures[7]>0,geometry_textures
     report['geometry_textures']=geometry_textures
     alpha_contacts=words(monitor,symbol('rf_scene_geometry_alpha_contacts'),4)
     assert alpha_contacts==expected('GEOMETRY_ALPHA_CONTACTS'),alpha_contacts
     assert alpha_contacts[0]==sum(alpha_contacts[1:]) and alpha_contacts[3]==0,alpha_contacts
     if args.actor_pairs and final_level.lower()=='l1s2.rfl':assert alpha_contacts[1]>0 and alpha_contacts[2]>0,alpha_contacts
     report['geometry_alpha_contacts']=alpha_contacts
     glare_solids=words(monitor,symbol('rf_scene_glare_solids'),5)
     assert glare_solids==expected('GLARE_SOLIDS') and glare_solids[3]==0,glare_solids
     if args.actor_pairs:assert glare_solids[0]>0 and glare_solids[1]>0 and glare_solids[2]>0,glare_solids
     report['glare_solids']=glare_solids
     npc_visibility=words(monitor,symbol('rf_scene_npc_visibility'),7)
     assert npc_visibility==expected('NPC_VISIBILITY') and npc_visibility[6]==0,npc_visibility
     # Two initial fixture snapshots, then volume and corona snapshots after
     # each completed room refresh (frame zero has neither live snapshot).
     if args.actor_pairs:assert npc_visibility[0]>0 and npc_visibility[1]>0 and (2+2*(section_frames-1))*npc_visibility[3]==npc_visibility[0],npc_visibility
     report['npc_visibility']=npc_visibility
     npc_rooms=words(monitor,symbol('rf_scene_npc_visibility_rooms'),6)
     assert npc_rooms==expected('NPC_VISIBILITY_ROOMS'),npc_rooms
     if args.actor_pairs:assert npc_rooms[0]==npc_visibility[0] and npc_rooms[1]>0,npc_rooms
     report['npc_visibility_rooms']=npc_rooms
     npc_room_refresh=words(monitor,symbol('rf_scene_npc_room_refresh'),8)
     assert npc_room_refresh==expected('NPC_ROOM_REFRESH') and npc_room_refresh[7]==0,npc_room_refresh
     assert npc_room_refresh[0]==section_frames-1,npc_room_refresh
     if args.actor_pairs and section_frames>1:assert npc_room_refresh[2]>0,npc_room_refresh
     if args.actor_pairs and section_frames>1 and final_level.lower()=='l1s2.rfl':assert npc_room_refresh[4]>0,npc_room_refresh
     report['npc_room_refresh']=npc_room_refresh
     npc_dispatch=words(monitor,symbol('rf_scene_npc_render_dispatch'),6)
     assert npc_dispatch==expected('NPC_RENDER_DISPATCH') and npc_dispatch[0]==section_frames and npc_dispatch[5]==0,npc_dispatch
     assert npc_dispatch[1]==npc_dispatch[2]+npc_dispatch[3],npc_dispatch
     report['npc_render_dispatch']=npc_dispatch
     clutter_dispatch=words(monitor,symbol('rf_scene_clutter_render_dispatch'),6)
     assert clutter_dispatch==expected('CLUTTER_RENDER_DISPATCH') and clutter_dispatch[0]==section_frames and clutter_dispatch[5]==0,clutter_dispatch
     assert clutter_dispatch[1]==clutter_dispatch[2]+clutter_dispatch[3],clutter_dispatch
     report['clutter_render_dispatch']=clutter_dispatch
     mover_visibility=words(monitor,symbol('rf_scene_mover_visibility'),3)
     assert mover_visibility==expected('MOVER_VISIBILITY') and mover_visibility[2]==0,mover_visibility
     report['mover_visibility']=mover_visibility
     glare_search=words(monitor,symbol('rf_scene_glare_search'),8)
     assert glare_search==expected('GLARE_SEARCH') and glare_search[7]==0,glare_search
     assert glare_search[0]==1 and glare_search[1]==glare_search[3],glare_search
     report['glare_search']=glare_search
     corona=words(monitor,symbol('rf_scene_corona_draw'),8)
     assert corona==expected('CORONA_DRAW') and corona[0]==section_frames and corona[7]==0,corona
     assert corona[1]==corona[2] and corona[3]<=corona[2] and corona[4]>=3*corona[3],corona
     assert corona[6]<=128*1024,corona
     report['corona_draw']=corona
     volume=words(monitor,symbol('rf_scene_volume_draw'),8)
     assert volume==expected('VOLUME_DRAW') and volume[0]==section_frames and volume[7]==0,volume
     assert volume[3]<=volume[2]<=volume[1] and volume[4]>=3*volume[3],volume
     report['volume_draw']=volume
     volume_test=words(monitor,symbol('rf_scene_volume_test'),8)
     assert volume_test==expected('VOLUME_TEST') and volume_test[6]==0,volume_test
     if args.volume_test:
      assert volume_test[:5]==[section_frames-90]*5 and volume[2]>=section_frames-90 and volume[3]>=section_frames-90,volume_test
     report['volume_test']=volume_test
     volume_npc=words(monitor,symbol('rf_scene_volume_npc_test'),8)
     assert volume_npc==expected('VOLUME_NPC_TEST') and volume_npc[6]==0,volume_npc
     if args.volume_test:assert volume_npc[:5]==[6,9,3,2,1],volume_npc
     report['volume_npc_test']=volume_npc
     glare_instances=words(monitor,symbol('rf_scene_glare_instances'),10)
     assert glare_instances==expected('GLARE_INSTANCES') and glare_instances[3]==glare_instances[8] and glare_instances[9]==0,glare_instances
     assert glare_instances[5]<=glare_instances[6]<=256*1024,glare_instances
     if final_level.lower()=='l1s1.rfl':
      glare_instance_reference=json.loads((root/'artifacts/clutter-scene-glares.json').read_text())
      assert glare_instance_reference['result']=='PASS' and glare_instances==glare_instance_reference['expected'],glare_instances
     assert glare_search[1]==glare_instances[3],(glare_search,glare_instances)
     report['glare_instances']=glare_instances
     attachment_motion=words(monitor,symbol('rf_scene_attachment_motion'),8)
     assert attachment_motion==expected('ATTACHMENT_MOTION') and attachment_motion[6]==0,attachment_motion
     if args.actor_pairs and section_frames>1:
      n=glare_instances[3]
      assert attachment_motion[:5]==[4*n,3*n,n,4*n,n],attachment_motion
     report['attachment_motion']=attachment_motion
     glare_loss=words(monitor,symbol('rf_scene_glare_loss_test'),8)
     assert glare_loss==expected('GLARE_LOSS_TEST') and glare_loss[6]==0,glare_loss
     if args.glare_loss:assert glare_loss==[3,3,3,3,3,1584,0,0],glare_loss
     report['glare_loss_test']=glare_loss
     glare_retirement=words(monitor,symbol('rf_scene_glare_retirement'),8)
     assert glare_retirement==expected('GLARE_RETIREMENT') and glare_retirement[7]==0,glare_retirement
     assert glare_retirement[0]==section_frames-1,glare_retirement
     assert glare_retirement[3]+glare_retirement[6]==glare_instances[3],glare_retirement
     report['glare_retirement']=glare_retirement
     attachments=words(monitor,symbol('rf_scene_attachments'),8)
     assert attachments==expected('ATTACHMENTS') and attachments[7]==0,attachments
     assert attachments[0]==section_frames-1 and attachments[5]<=32768,attachments
     if section_frames>1:
      assert attachments[3]==glare_instances[3] and attachments[4]==glare_instances[0],attachments
     report['attachments']=attachments
     glare_rooms=words(monitor,symbol('rf_scene_glare_rooms'),8)
     assert glare_rooms==expected('GLARE_ROOMS') and glare_rooms[7]==0,glare_rooms
     assert glare_rooms[0]==section_frames-1 and glare_rooms[1]==(section_frames-1)*glare_instances[3]-(max(0,section_frames-91)*3 if args.glare_loss else 0),glare_rooms
     if section_frames>1:
      assert glare_rooms[2]==glare_instances[3] and glare_rooms[3]>0,glare_rooms
      assert glare_rooms[5]==20*glare_instances[3] and glare_rooms[5]<=65536,glare_rooms
     report['glare_rooms']=glare_rooms
     clutter_tag_queries=words(monitor,symbol('rf_scene_clutter_tag_queries'),5)
     assert clutter_tag_queries==expected('CLUTTER_TAG_QUERIES') and clutter_tag_queries[4]==0,clutter_tag_queries
     if final_level.lower()=='l1s1.rfl':
      tag_query_reference=json.loads((root/'artifacts/clutter-scene-tag-queries.json').read_text())
      assert tag_query_reference['result']=='PASS' and clutter_tag_queries==tag_query_reference['expected'],clutter_tag_queries
     report['clutter_tag_queries']=clutter_tag_queries
     clutter_materials=words(monitor,symbol('rf_scene_clutter_materials'),8)
     assert clutter_materials==expected('CLUTTER_MATERIALS'),clutter_materials
     clutter_skins=words(monitor,symbol('rf_scene_clutter_skins'),6)
     assert clutter_skins==expected('CLUTTER_SKINS') and clutter_skins[1]==clutter_render[1],clutter_skins
     assert clutter_materials[0]==clutter_skins[0],clutter_materials
     assert clutter_materials[3]<=clutter_materials[4]<=1024*1024,clutter_materials
     if final_level.lower()=='l1s1.rfl':
      skin_reference=json.loads((root/'artifacts/clutter-scene-skins.json').read_text())
      assert skin_reference['result']=='PASS' and clutter_skins==skin_reference['expected'],clutter_skins
      assert clutter_materials[:5]==[12,23,12,909700,915680],clutter_materials
     report['clutter_materials']=clutter_materials
     report['clutter_skins']=clutter_skins
     clutter_draw=words(monitor,symbol('rf_scene_clutter_draw'),6)
     assert clutter_draw==expected('CLUTTER_DRAW') and clutter_draw[0]==section_frames,clutter_draw
     assert clutter_draw[4]==0 and clutter_draw[2]%3==0,clutter_draw
     report['clutter_draw']=clutter_draw
     clutter_bodies=words(monitor,symbol('rf_scene_clutter_bodies'),10)
     assert clutter_bodies==expected('CLUTTER_BODIES'),clutter_bodies
     assert clutter_bodies[0]==clutter_bodies[6]==clutter_bodies[7]==clutter_render[1],clutter_bodies
     assert clutter_bodies[8:]==[0,0] and clutter_bodies[3]<=clutter_bodies[4]<=256*1024,clutter_bodies
     if final_level.lower()=='l1s1.rfl':
      body_reference=json.loads((root/'artifacts/clutter-scene-bodies.json').read_text())
      assert body_reference['result']=='PASS' and clutter_bodies[:5]==body_reference['expected'],clutter_bodies
     report['clutter_bodies']=clutter_bodies
     clutter_collision=words(monitor,symbol('rf_scene_clutter_collision'),9)
     assert clutter_collision==expected('CLUTTER_COLLISION') and clutter_collision[8]==0,clutter_collision
     assert clutter_collision[0]==clutter_render[0] and clutter_collision[4]==6*clutter_bodies[0],clutter_collision
     if final_level.lower()=='l1s1.rfl':
      collision_reference=json.loads((root/'artifacts/clutter-scene-collision.json').read_text())
      assert collision_reference['result']=='PASS' and clutter_collision==collision_reference['expected'],clutter_collision
     report['clutter_collision']=clutter_collision
     clutter_visibility=words(monitor,symbol('rf_scene_clutter_visibility'),2)
     assert clutter_visibility==expected('CLUTTER_VISIBILITY') and clutter_visibility[0]==clutter_bodies[0],clutter_visibility
     report['clutter_visibility']=clutter_visibility
     npc_models=words(monitor,symbol('rf_scene_npc_models'),4)
     assert npc_models==expected('NPC_MODELS'),npc_models
     assert npc_models==[npc_bodies[1],npc_bodies[0]*80,npc_bodies[1],0],npc_models
     report['npc_models']=npc_models
     collision_cache=words(monitor,symbol('rf_scene_npc_collision_cache'),6)
     assert collision_cache==expected('NPC_COLLISION_CACHE'),collision_cache
     assert collision_cache[0]==collision_cache[4]==npc_models[0] and collision_cache[1]<=256*1024 and collision_cache[5]==0,collision_cache
     if section_frames>1:assert collision_cache[2]>0,collision_cache
     report['npc_collision_cache']=collision_cache
     model_queries=words(monitor,symbol('rf_scene_npc_model_queries'),7)
     assert model_queries==expected('NPC_MODEL_QUERIES') and model_queries[1]+model_queries[2]<=1024*1024 and model_queries[6]==0,model_queries
     if args.actor_pairs and section_frames>1:assert model_queries[3]>0 and model_queries[4]>0,model_queries
     report['npc_model_queries']=model_queries
     impact_groups=words(monitor,symbol('rf_scene_npc_impact_groups'),3)
     assert impact_groups==expected('NPC_IMPACT_GROUPS') and impact_groups[1]==impact_groups[0]*4,impact_groups
     report['npc_impact_groups']=impact_groups
     npc_crouch=words(monitor,symbol('rf_scene_npc_crouch_test'),7)
     assert npc_crouch==expected('NPC_CROUCH_TEST') and npc_crouch[5]==0,npc_crouch
     if args.actor_pairs:assert npc_crouch[0]>0 and npc_crouch[1]==npc_crouch[0] and npc_crouch[2]==npc_crouch[3]*2 and npc_crouch[0]==npc_crouch[3]*3,npc_crouch
     report['npc_crouch']=npc_crouch
     npc_normal=words(monitor,symbol('rf_scene_npc_normal_test'),4)
     assert npc_normal==expected('NPC_NORMAL_TEST') and npc_normal[3]==0,npc_normal
     if args.actor_pairs:assert npc_normal[0]>0 and npc_normal[1]>0,npc_normal
     report['npc_normal']=npc_normal
     npc_slow=words(monitor,symbol('rf_scene_npc_slow_test'),4)
     assert npc_slow==expected('NPC_SLOW_TEST') and npc_slow[3]==0,npc_slow
     if args.actor_pairs:assert npc_slow[0]>0 and npc_slow[1]>0,npc_slow
     report['npc_slow']=npc_slow
     npc_stand=words(monitor,symbol('rf_scene_npc_stand_test'),7)
     assert npc_stand==expected('NPC_STAND_TEST') and npc_stand[5]==0,npc_stand
     if args.actor_pairs:assert npc_stand[0]>0 and npc_stand[0]==npc_stand[1]+npc_stand[2] and npc_stand[1]==npc_stand[3],npc_stand
     report['npc_stand']=npc_stand
     npc_fall=words(monitor,symbol('rf_scene_npc_fall_test'),6)
     assert npc_fall==expected('NPC_FALL') and npc_fall[5]==0,npc_fall
     if args.actor_pairs:assert npc_fall[0]>0 and npc_fall[1]==npc_fall[2] and npc_fall[3]==npc_fall[1]*2 and npc_fall[0]==npc_fall[1]*4,npc_fall
     report['npc_fall']=npc_fall
     ground_query=words(monitor,symbol('rf_scene_npc_ground_query_test'),6)
     assert ground_query==expected('NPC_GROUND_QUERY') and ground_query[4]==0,ground_query
     if args.actor_pairs:assert ground_query[0]>0 and ground_query[1]>0 and ground_query[2]>0,ground_query
     report['npc_ground_query']=ground_query
     unholster=words(monitor,symbol('rf_scene_npc_unholster'),4)
     assert unholster==expected('NPC_UNHOLSTER') and unholster[2]==4*unholster[0] and unholster[1]<=unholster[0],unholster
     report['npc_unholster']=unholster
     unholster_test=words(monitor,symbol('rf_scene_npc_unholster_test'),5)
     assert unholster_test==expected('NPC_UNHOLSTER_TEST') and unholster_test[4]==0,unholster_test
     if args.actor_pairs:assert unholster_test[0]>0 and unholster_test[1]==unholster_test[0],unholster_test
     report['npc_unholster_test']=unholster_test
     ai_reset=words(monitor,symbol('rf_scene_npc_ai_reset_test'),5)
     assert ai_reset==expected('NPC_AI_RESET') and ai_reset[4]==0,ai_reset
     if args.actor_pairs:assert ai_reset[0]>0 and ai_reset[1]==8*ai_reset[0] and ai_reset[2]==4*ai_reset[0],ai_reset
     report['npc_ai_reset']=ai_reset
     motion_request=words(monitor,symbol('rf_scene_npc_motion_request_test'),5)
     assert motion_request==expected('NPC_MOTION_REQUEST') and motion_request[4]==0,motion_request
     if args.actor_pairs:assert motion_request[0]>0 and motion_request[1]==motion_request[0]*8 and motion_request[2]==motion_request[0]*4,motion_request
     report['npc_motion_request']=motion_request
     support_refresh=words(monitor,symbol('rf_scene_npc_support_refresh'),6)
     assert support_refresh==expected('NPC_SUPPORT_REFRESH') and support_refresh[5]==0,support_refresh
     if args.actor_pairs:assert support_refresh[0]>0 and support_refresh[3]==80 and support_refresh[2]>0,support_refresh
     report['npc_support_refresh']=support_refresh
     body_sweep=words(monitor,symbol('rf_scene_npc_body_sweep_test'),5)
     assert body_sweep==expected('NPC_BODY_SWEEP') and body_sweep[4]==0,body_sweep
     if args.actor_pairs:assert body_sweep[0]>0 and body_sweep[1]>0 and body_sweep[2]>0,body_sweep
     report['npc_body_sweep']=body_sweep
     pose_demand=words(monitor,symbol('rf_scene_npc_pose_demand'),5)
     assert pose_demand==expected('NPC_POSE_DEMAND') and pose_demand[2]==0 and pose_demand[4]==0,pose_demand
     if args.actor_pairs:assert pose_demand[1]>=pose_demand[3] and pose_demand[3]>0,pose_demand
     report['npc_pose_demand']=pose_demand
     actor_model=words(monitor,symbol('rf_scene_actor_model_test'),6)
     assert actor_model==expected('ACTOR_MODEL_TEST'),actor_model
     if args.actor_pairs:assert actor_model[0]==24 and actor_model[1]>0 and actor_model[2]>=12 and actor_model[4:]==[0,3],actor_model
     report['actor_model_test']=actor_model
     pair_dispatch=words(monitor,symbol('rf_scene_pair_dispatch'),5)
     assert pair_dispatch==expected('PAIR_DISPATCH'),pair_dispatch
     if args.actor_pairs:assert pair_dispatch==[27,1,2,24,0],pair_dispatch
     report['pair_dispatch']=pair_dispatch
     collision_views=words(monitor,symbol('rf_scene_collision_views'),8)
     assert collision_views==expected('COLLISION_VIEWS'),collision_views
     assert collision_views[0]==section_frames and collision_views[3]==section_frames*active_npc_count,collision_views
     assert collision_views[4]==0xffffffff and collision_views[5]==active_npc_count and collision_views[6]==0 and collision_views[7]==section_frames-1,collision_views
     report['collision_views']=collision_views
     collision_responses=words(monitor,symbol('rf_scene_collision_responses'),6)
     assert collision_responses==expected('COLLISION_RESPONSES'),collision_responses
     assert collision_responses[0]==section_frames and collision_responses[3]==section_frames*active_npc_count,collision_responses
     assert collision_responses[4]==0 and collision_responses[5]==section_frames-1,collision_responses
     report['collision_responses']=collision_responses
     actor_pairs=words(monitor,symbol('rf_scene_actor_pair_test'),8)
     assert actor_pairs==expected('ACTOR_PAIR_TEST'),actor_pairs
     if args.actor_pairs:assert actor_pairs[:4]==[18,9,9,3] and actor_pairs[5:]==[0,3,0],actor_pairs
     else:assert actor_pairs==[0]*8,actor_pairs
     report['actor_pair_test']=actor_pairs
     npc_damage_owners=words(monitor,symbol('rf_scene_npc_damage_owners'),3)
     assert npc_damage_owners==expected('NPC_DAMAGE_OWNERS'),npc_damage_owners
     report['npc_damage_owners']=npc_damage_owners
     npc_death_owners=words(monitor,symbol('rf_scene_npc_death_owners'),3)
     assert npc_death_owners==expected('NPC_DEATH_OWNERS'),npc_death_owners
     death_hash=2166136261
     for value in (bytes([255])*20+bytes(4))*npc_bodies[1]:death_hash=((death_hash^value)*16777619)&0xffffffff
     assert npc_death_owners==[npc_bodies[1],npc_bodies[0]*24,death_hash],npc_death_owners
     report['npc_death_owners']=npc_death_owners
     npc_pain_owners=words(monitor,symbol('rf_scene_npc_pain_owners'),4)
     assert npc_pain_owners==expected('NPC_PAIN_OWNERS'),npc_pain_owners
     assert npc_pain_owners[0]==npc_bodies[1] and npc_pain_owners[1]==npc_bodies[0]*16,npc_pain_owners
     report['npc_pain_owners']=npc_pain_owners
     npc_pain_sound_owners=words(monitor,symbol('rf_scene_npc_pain_sound_owners'),4)
     assert npc_pain_sound_owners==expected('NPC_PAIN_SOUND_OWNERS'),npc_pain_sound_owners
     npc_eyes=words(monitor,symbol('rf_scene_npc_eyes'),4)
     assert npc_eyes==expected('NPC_EYES') and npc_eyes[0]==active_npc_count,npc_eyes
     report['npc_eyes']=npc_eyes
     assert npc_pain_sound_owners[0]==npc_bodies[1] and npc_pain_sound_owners[1]==npc_bodies[0]*8,npc_pain_sound_owners
     report['npc_pain_sound_owners']=npc_pain_sound_owners
     npc_pain_test=words(monitor,symbol('rf_scene_npc_pain_test_words'),10)
     assert npc_pain_test==expected('NPC_PAIN_TEST'),npc_pain_test
     report['npc_pain_test']=npc_pain_test
     death_animation=words(monitor,symbol('rf_scene_death_animation_test'),8)
     action_audio=words(monitor,symbol('rf_scene_npc_action_audio'),9)
     impact_dispatch=words(monitor,symbol('rf_scene_npc_impact_dispatch'),6)
     impact_test=words(monitor,symbol('rf_scene_npc_impact_test'),4)
     assert impact_dispatch==expected('NPC_IMPACT_DISPATCH') and impact_dispatch[5]==0,impact_dispatch
     assert impact_test==expected('NPC_IMPACT_TEST') and impact_test[3]==0,impact_test
     if args.damage_uid==8456:assert impact_dispatch==[3,1,1,0,1,0] and impact_test[0]==4,(impact_dispatch,impact_test)
     report['npc_impact_dispatch']=impact_dispatch;report['npc_impact_test']=impact_test
     impact_audio=words(monitor,symbol('rf_scene_npc_impact_audio'),12)
     assert impact_audio==expected('NPC_IMPACT_AUDIO') and impact_audio[7]==0,impact_audio
     if args.damage_uid==8456:assert impact_audio[:4]==[1,1,1,1] and impact_audio[4]>0,impact_audio
     report['npc_impact_audio']=impact_audio
     destroy_audio=words(monitor,symbol('rf_scene_npc_contact_destroy_audio'),12)
     destroy_test=words(monitor,symbol('rf_scene_npc_contact_destroy_test'),4)
     assert destroy_audio==expected('NPC_CONTACT_DESTROY_AUDIO') and destroy_audio[7]==0,destroy_audio
     assert destroy_test==expected('NPC_CONTACT_DESTROY_TEST') and destroy_test[3]==0,destroy_test
     if args.damage_uid==8456:assert destroy_test[0]==4 and destroy_audio[:3]==[1,1,1],(destroy_test,destroy_audio)
     report['npc_contact_destroy_audio']=destroy_audio;report['npc_contact_destroy_test']=destroy_test
     contact_audio=words(monitor,symbol('rf_scene_npc_contact_sound_audio'),12)
     contact_test=words(monitor,symbol('rf_scene_npc_contact_sound_test'),4)
     assert contact_audio==expected('NPC_CONTACT_SOUND_AUDIO') and contact_audio[7]==0,contact_audio
     assert contact_test==expected('NPC_CONTACT_SOUND_TEST') and contact_test[3]==0,contact_test
     if args.damage_uid==8456:assert contact_test[0]==4 and contact_audio[:3]==[3,1,1],(contact_test,contact_audio)
     report['npc_contact_sound_audio']=contact_audio;report['npc_contact_sound_test']=contact_test
     driller_feedback=words(monitor,symbol('rf_scene_driller_feedback_test'),8)
     assert driller_feedback==expected('DRILLER_FEEDBACK_TEST') and driller_feedback[7]==0,driller_feedback
     if args.damage_uid==8456:assert driller_feedback==[4,3,2,2,2,122880,600,0],driller_feedback
     report['driller_feedback']=driller_feedback
     inventory_owners=words(monitor,symbol('rf_scene_npc_inventory_owners'),4)
     assert inventory_owners==expected('NPC_INVENTORY_OWNERS') and inventory_owners[0]>0,inventory_owners
     report['npc_inventory_owners']=inventory_owners
     startup_weapons=words(monitor,symbol('rf_scene_npc_startup_weapons'),4)
     assert startup_weapons==expected('NPC_STARTUP_WEAPONS') and startup_weapons[0]>0 and startup_weapons[2]>=startup_weapons[0],startup_weapons
     report['npc_startup_weapons']=startup_weapons
     weapon_models=words(monitor,symbol('rf_scene_weapon_models'),8)
     assert weapon_models==expected('WEAPON_MODELS'),weapon_models
     assert 0<weapon_models[0]<=weapon_models[1] and weapon_models[2]<=weapon_models[3]<=256*1024,weapon_models
     report['weapon_models']=weapon_models
     weapon_materials=words(monitor,symbol('rf_scene_weapon_materials'),8)
     assert weapon_materials==expected('WEAPON_MATERIALS'),weapon_materials
     assert weapon_materials[0]==weapon_models[0] and weapon_materials[2]>0 and weapon_materials[3]<=weapon_materials[4]<=512*1024,weapon_materials
     report['weapon_materials']=weapon_materials
     weapon_muzzle=words(monitor,symbol('rf_scene_weapon_muzzle'),4)
     assert weapon_muzzle==expected('WEAPON_MUZZLE') and weapon_muzzle[1]>0,weapon_muzzle
     report['weapon_muzzle']=weapon_muzzle
     weapon_aim=words(monitor,symbol('rf_scene_weapon_aim'),4)
     assert weapon_aim==expected('WEAPON_AIM') and weapon_aim[1]>0,weapon_aim
     report['weapon_aim']=weapon_aim
     weapon_hands=words(monitor,symbol('rf_scene_weapon_hands'),4)
     assert weapon_hands==expected('WEAPON_HANDS'),weapon_hands
     report['weapon_hands']=weapon_hands
     weapon_draw=words(monitor,symbol('rf_scene_weapon_draw'),6)
     assert weapon_draw==expected('WEAPON_DRAW'),weapon_draw
     if args.actor_uid is not None:assert weapon_draw[3]>0,weapon_draw
     report['weapon_draw']=weapon_draw
     weapon_placement=words(monitor,symbol('rf_scene_weapon_placement'),4)
     assert weapon_placement==expected('WEAPON_PLACEMENT') and weapon_placement[1]>0,weapon_placement
     report['weapon_placement']=weapon_placement
     weapon_supply=words(monitor,symbol('rf_scene_weapon_supply'),4)
     assert weapon_supply==expected('WEAPON_SUPPLY') and weapon_supply[:3]==[44,40,4872],weapon_supply
     report['weapon_supply']=weapon_supply
     weapon_reset=words(monitor,symbol('rf_scene_weapon_reset_catalog'),4)
     assert weapon_reset==expected('WEAPON_RESET_CATALOG') and weapon_reset==[44,7,4872,3940996059],weapon_reset
     report['weapon_reset_catalog']=weapon_reset
     contact_splashes=words(monitor,symbol('rf_scene_contact_splash_assets'),8)
     assert contact_splashes==expected('CONTACT_SPLASH_ASSETS'),contact_splashes
     assert contact_splashes[:2]==[43,44] and contact_splashes[4]==1016 and contact_splashes[6:]==[1,1],contact_splashes
     report['contact_splash_assets']=contact_splashes
     contact_dispatch=words(monitor,symbol('rf_scene_contact_dispatch_test'),4)
     assert contact_dispatch==expected('CONTACT_DISPATCH_TEST') and contact_dispatch[3]==0,contact_dispatch
     if args.damage_uid==8456:assert contact_dispatch==[4,3,305419896,0],contact_dispatch
     report['contact_dispatch']=contact_dispatch
     object_contact=words(monitor,symbol('rf_scene_object_contact_test'),4)
     assert object_contact==expected('OBJECT_CONTACT_TEST') and object_contact[3]==0,object_contact
     if args.damage_uid==8456:assert object_contact==[4,2,1,0],object_contact
     report['object_contact']=object_contact
     player_contact_audio=words(monitor,symbol('rf_scene_player_contact_audio'),12)
     player_contact_test=words(monitor,symbol('rf_scene_player_contact_test'),4)
     assert player_contact_audio==expected('PLAYER_CONTACT_AUDIO') and player_contact_audio[7]==0,player_contact_audio
     assert player_contact_test==expected('PLAYER_CONTACT_TEST') and player_contact_test[3]==0,player_contact_test
     if args.damage_uid==8456:assert player_contact_test[0]==2 and player_contact_audio[0]==1,(player_contact_test,player_contact_audio)
     report['player_contact_audio']=player_contact_audio;report['player_contact_test']=player_contact_test
     clutter_contact=words(monitor,symbol('rf_scene_clutter_contact_test'),8)
     assert clutter_contact==expected('CLUTTER_CONTACT_TEST') and clutter_contact[7]==0,clutter_contact
     if args.damage_uid==8456:assert clutter_contact[0]==3 and clutter_contact[4:]==[2,1,2,0],clutter_contact
     report['clutter_contact']=clutter_contact


     assert death_animation==expected('DEATH_ANIMATION_TEST'),death_animation
     assert action_audio==expected('NPC_ACTION_AUDIO'),action_audio
     if args.death_animation:assert death_animation[0]==120 and death_animation[6]==0 and action_audio[7]==0,(death_animation,action_audio)
     if args.death_animation and args.damage_uid==8456:assert action_audio[:4]==[1,1,1,1] and action_audio[4]>0,action_audio
     report['death_animation']=death_animation;report['npc_action_audio']=action_audio
     pain_audio=words(monitor,symbol('rf_scene_npc_pain_audio'),9)
     pain_sound_test=words(monitor,symbol('rf_scene_npc_pain_sound_test'),10)
     assert pain_audio==expected('NPC_PAIN_AUDIO') and pain_audio[7]==0,pain_audio
     assert pain_sound_test==expected('NPC_PAIN_SOUND_TEST'),pain_sound_test
     if args.damage_uid==8456:assert pain_audio[:4]==[3,1,1,1] and pain_audio[4]>0,pain_audio
     report['npc_pain_audio']=pain_audio;report['npc_pain_sound_test']=pain_sound_test
     player_pain_audio=words(monitor,symbol('rf_scene_player_pain_audio'),9)
     player_pain_test=words(monitor,symbol('rf_scene_player_pain_test'),21)
     assert player_pain_audio==expected('PLAYER_PAIN_AUDIO') and player_pain_audio[7]==0,player_pain_audio
     assert player_pain_test==expected('PLAYER_PAIN_TEST'),player_pain_test
     if args.damage_uid==8456:
      assert player_pain_audio[:3]==[5,3,3] and player_pain_audio[4]>0,player_pain_audio
      assert player_pain_test[:7]==player_pain_test[7:14],player_pain_test
      assert [player_pain_test[i] for i in (0,7,14)]==[2000,2000,3000],player_pain_test
      assert [player_pain_test[i] for i in (1,8,15)]==[0xffffffff]*3,player_pain_test
      assert [player_pain_test[i] for i in (6,13,20)]==[1,1,2],player_pain_test
     report['player_pain_audio']=player_pain_audio;report['player_pain_test']=player_pain_test
     player_damage_audio=words(monitor,symbol('rf_scene_player_damage_audio_test'),18)
     assert player_damage_audio==expected('PLAYER_DAMAGE_AUDIO_TEST'),player_damage_audio
     if args.damage_uid==8456:
      for step,(health,armor,amount) in enumerate(((95.2,94.8,10),(90.4,89.6,10),(71.2,68.8,40))):
       row=player_damage_audio[step*6:step*6+6]
       actual=struct.unpack('<3f',struct.pack('<3I',*row[:3]))
       assert all(abs(a-b)<.0001 for a,b in zip(actual,(health,armor,amount))) and row[3:]==[128,1,0],row
     report['player_damage_audio_test']=player_damage_audio
     player_death_audio=words(monitor,symbol('rf_scene_player_death_audio_test'),18)
     assert player_death_audio==expected('PLAYER_DEATH_AUDIO_TEST'),player_death_audio
     if args.damage_uid==8456:
      for step,health in enumerate((-160,-460)):
       row=player_death_audio[step*9:step*9+9]
       actual=struct.unpack('<3f',struct.pack('<3I',*row[:3]))
       assert all(abs(a-b)<.001 for a,b in zip(actual,(health,0,300))),row
       assert row[3:6]==[4,0 if step else 128,3] and row[7:]==[3000,0xffffffff],row
      assert player_death_audio[6]==player_death_audio[15],player_death_audio
     report['player_death_audio_test']=player_death_audio
     npc_damage_test=words(monitor,symbol('rf_scene_npc_damage_test_words'),64)
     assert npc_damage_test==expected('NPC_DAMAGE_TEST'),npc_damage_test
     assert npc_damage_test[0]==npc_damage_test[63]==0,npc_damage_test
     report['npc_damage_test']=npc_damage_test
     death_clearance=words(monitor,symbol('rf_scene_death_clearance_test'),8)
     assert death_clearance==expected('DEATH_CLEARANCE'),death_clearance
     if args.damage_uid is not None:
      passes=int(section_frames>1)+int(section_frames>90)
      assert death_clearance[0]==passes and death_clearance[6]==0,death_clearance
      assert death_clearance[1]==2*passes*death_clearance[4] and sum(death_clearance[2:4])==death_clearance[1],death_clearance
      assert death_clearance[7]==20*death_clearance[4],death_clearance
     report['death_clearance']=death_clearance
     npc_registration=words(monitor,symbol('rf_scene_npc_registration'),6)
     assert npc_registration==expected('NPC_REGISTRATION'),npc_registration
     assert npc_registration[0]==npc_registration[5]==npc_bodies[1],npc_registration
     report['npc_registration']=npc_registration
     npc_links=words(monitor,symbol('rf_scene_npc_links'),4)
     assert npc_links==expected('NPC_LINKS'),npc_links
     report['npc_links']=npc_links
     npc_backlinks=words(monitor,symbol('rf_scene_npc_backlinks'),4)
     assert npc_backlinks==expected('NPC_BACKLINKS'),npc_backlinks
     report['npc_backlinks']=npc_backlinks

     npc_support=words(monitor,symbol('rf_scene_npc_support'),12)
     assert npc_support==expected('NPC_SUPPORT_PROBE'),npc_support
     assert npc_support[0]==npc_bodies[1] and npc_support[1]+npc_support[2]==npc_support[0],npc_support
     report['npc_support_probe']=npc_support
     npc_support_miss=words(monitor,symbol('rf_scene_npc_support_first_miss'),16)
     assert npc_support_miss==expected('NPC_SUPPORT_FIRST_MISS'),npc_support_miss
     report['npc_support_first_miss']=npc_support_miss
     npc_deep=words(monitor,symbol('rf_scene_npc_support_deep'),8)
     npc_deep_first=words(monitor,symbol('rf_scene_npc_support_deep_first'),20)
     assert npc_deep==expected('NPC_SUPPORT_DEEP') and npc_deep_first==expected('NPC_SUPPORT_DEEP_FIRST')
     assert npc_deep[0]==npc_support[3],npc_deep
     report['npc_support_deep']=npc_deep;report['npc_support_deep_first']=npc_deep_first



     player=words(monitor,symbol('rf_scene_campaign_player'),4)
     assert player==expected('CAMPAIGN_PLAYER') and player[1:3]==[0,8],player
     report['campaign_player']=player
     player_vitals=words(monitor,symbol('rf_scene_player_vitals'),6)
     assert player_vitals==expected('PLAYER_VITALS'),player_vitals
     report['player_vitals']=player_vitals
     trigger_contacts=words(monitor,symbol('rf_scene_trigger_contacts'),6)
     assert trigger_contacts==expected('TRIGGER_CONTACTS') and trigger_contacts[5]==0,trigger_contacts
     if args.door:assert trigger_contacts[3]>0,trigger_contacts
     report['trigger_contacts']=trigger_contacts
     activation=words(monitor,symbol('rf_scene_live_activation'),8)
     assert activation==expected('LIVE_ACTIVATION') and activation[5]==0,activation
     if args.door:assert activation[2]==2 and activation[6]>0 and activation[6]==activation[7],activation
     report['live_activation']=activation
     motion=words(monitor,symbol('rf_scene_live_motion'),8)
     assert motion==expected('LIVE_MOTION') and motion[7]==0,motion
     door_positions=words(monitor,symbol('rf_scene_live_door_positions'),6)
     assert door_positions==expected('DOOR_POSITIONS'),door_positions
     report['live_motion']=motion;report['door_positions']=door_positions
     audio=words(monitor,symbol('rf_scene_live_audio'),8)
     assert audio==expected('LIVE_AUDIO') and audio[3]==0,audio
     assert audio[6]==motion[0]*800,audio
     if args.door:assert audio[4]>=2,audio
     report['live_audio']=audio
     foley=words(monitor,symbol('rf_scene_foley'),10)
     assert foley==expected('FOLEY') and foley[:3]==[497,1140,1],foley
     assert foley[3]==26452+foley[8]*56 and foley[4]<512*1024 and foley[5]==1135,foley
     assert foley[6]==1274782420 and foley[7]==3317977305,foley
     report['foley']=foley
     pain_groups=words(monitor,symbol('rf_scene_npc_pain_groups'),3)
     assert pain_groups==expected('NPC_PAIN_GROUPS') and pain_groups[:2]==[foley[8],foley[8]*8],pain_groups
     report['npc_pain_groups']=pain_groups
     bank=words(monitor,symbol('rf_scene_sound_bank'),4)
     assert bank==expected('SOUND_BANK') and bank[2]+bank[3]==audio[1],bank
     if args.door:assert bank[0]==88 and bank[1]>=4 and bank[2]==111904+ambient_audio[5]+pain_audio[4]+player_pain_audio[4]+action_audio[4]+impact_audio[4]+destroy_audio[4]+contact_audio[4]+player_contact_audio[4] and audio[0]==foley[5]+12,bank
     report['sound_bank']=bank
     switch_audio=words(monitor,symbol('rf_scene_switch_audio'),4)
     assert switch_audio==expected('SWITCH_AUDIO') and switch_audio[0]==switches[0],switch_audio
     report['switch_audio']=switch_audio
     spatial=words(monitor,symbol('rf_scene_spatial_audio'),6)
     assert spatial==expected('SPATIAL_AUDIO'),spatial
     if args.door:assert spatial[0]>0 and spatial[1]>0 and spatial[3]>0 and spatial[4]>spatial[0],spatial
     report['spatial_audio']=spatial
     controller_audio=words(monitor,symbol('rf_scene_controller_audio'),4)
     assert controller_audio==expected('CONTROLLER_AUDIO'),controller_audio
     report['controller_audio']=controller_audio
     body_sweeps=words(monitor,symbol('rf_scene_actor_body_sweeps'),5)
     assert body_sweeps==expected('BODY_SWEEPS') and body_sweeps[3]==0,body_sweeps
     report['body_sweeps']=body_sweeps
     if args.door:
      traversal=measure(expected('PLAYER_SPAWN'),expected('PC_PLAY_BODY'))
      assert body_sweeps[2]>0 or motion[2]>0 or (motion[4]>=2 and traversal['crossed']),'Neither door contact, occupied hold nor traversal observed'
      report['door_traversal']=traversal
      contact=words(monitor,symbol('rf_scene_actor_body_contact'),23)
      assert contact==expected('BODY_CONTACT'),contact
      report['door_contact']=contact
     ground_queries=words(monitor,symbol('rf_scene_actor_ground_queries'),4)
     assert ground_queries==expected('GROUND_QUERIES') and ground_queries[3]==0,ground_queries
     report['ground_queries']=ground_queries
     groups=words(monitor,symbol('rf_scene_campaign_groups'),5)
     assert groups==expected('CAMPAIGN_GROUPS'),groups
     report['campaign_groups']=groups
     links=words(monitor,symbol('rf_scene_campaign_links'),4)
     assert links==expected('CAMPAIGN_LINKS')
     report['campaign_links']=links
     event_links=words(monitor,symbol('rf_scene_campaign_event_links'),4)
     assert event_links==expected('CAMPAIGN_EVENT_LINKS')
     report['campaign_event_links']=event_links
     event_ticks=words(monitor,symbol('rf_scene_event_ticks'),12)
     assert event_ticks==expected('CAMPAIGN_EVENT_TICKS')
     report['campaign_event_ticks']=event_ticks
     startup=words(monitor,symbol('rf_scene_startup_events'),9)+words(monitor,symbol('rf_scene_startup_gravity'),4)
     assert startup==expected('CAMPAIGN_STARTUP')
     report['campaign_startup']=startup
     spawn=words(monitor,symbol('rf_scene_player_spawn_diagnostic'),19);assert spawn==expected('PLAYER_SPAWN');report['player_spawn']=spawn
     for name,label,count in [('rf_scene_actor_initial_animation','PLAYER_INITIAL_ANIMATION',12),('rf_scene_actor_initial_eye_offsets','PLAYER_CLASS_EYE',6),('rf_scene_actor_stance_cache','PLAYER_CLASS_STANCE',50),('rf_scene_actor_selector_frames','PLAYER_STANCE_FRAMES',512),('rf_scene_actor_locomotion_frames','PLAYER_MOTION_FRAMES',768),('rf_scene_player_jump','PLAYER_JUMP',4),('rf_scene_player_jump_frames','PLAYER_JUMP_FRAMES',1024),('rf_scene_player_climb','PLAYER_CLIMB',8),('rf_scene_player_climb_frames','PLAYER_CLIMB_FRAMES',1152)]:
      got=words(monitor,symbol(name),count);assert got==expected(label),name;report[name]=got
    if args.climb:
     import struct
     assert expected('PLAYER_CLIMB')[1]>0 and expected('PLAYER_CLIMB')[2]>0,'Climb transitions missing'
     values=expected('PLAYER_CLIMB_FRAMES');climbing=[values[i:i+9] for i in range(0,len(values),9) if values[i] and values[i+2]==2]
     heights=[struct.unpack('<f',struct.pack('<I',r[4]))[0] for r in climbing]
     report['climbing_ticks']=len(climbing);report['climb_vertical_distance']=max(heights)-min(heights) if heights else 0
     assert report['climb_vertical_distance']>1,'Climb ascent missing'
     if args.approach:
      timeline=sorted([values[i:i+9] for i in range(0,len(values),9) if values[i]])
      entry=min(r[0] for r in timeline if r[2]==2)
      report['retained_outside_ticks']=sum(r[0]<entry and r[1]==0xffffffff and r[2]==1 for r in timeline)
      positions=[struct.unpack('<3f',struct.pack('<3I',*r[3:6])) for r in timeline if r[0]<entry and r[1]==0xffffffff and r[2]==1]
      report['outside_distance']=sum((positions[-1][i]-positions[0][i])**2 for i in range(3))**.5 if len(positions)>1 else 0
      assert report['outside_distance']>.25,'Walking approach missing'
    report['available_pages_at_completion']=d[44]
    goal_words=words(monitor,symbol('rf_scene_mission_goals'),4225)
    assert goal_words[0]<=64
    goal_bytes=struct.pack('<4225I',*goal_words);native_goals=[]
    for goal_index in range(goal_words[0]):
     offset=4+goal_index*264
     name=goal_bytes[offset:offset+256].split(b'\0')[0].decode('cp1252')
     value,persistent=struct.unpack_from('<iI',goal_bytes,offset+256)
     native_goals.append(f'MISSION_GOAL {name} {value} {persistent}')
    assert native_goals==[line for line in pc.stdout.splitlines() if line.startswith('MISSION_GOAL ')]
    report['mission_goals']=native_goals
    pickup_words=words(monitor,symbol('rf_scene_campaign_pickups'),5122)
    level_count,pickup_count=pickup_words[:2];assert level_count<=128 and pickup_count<=1024
    pickup_bytes=struct.pack('<5122I',*pickup_words);taken=[]
    for index in range(pickup_count):
     level_index,uid,retired=struct.unpack_from('<III',pickup_bytes,8200+index*12)
     assert level_index<level_count and retired<=1
     if retired:
      name=pickup_bytes[8+level_index*64:8+(level_index+1)*64].split(b'\0')[0].decode('ascii')
      taken.append(f'TAKEN_PICKUP {name} {uid}')
    assert taken==[line for line in pc.stdout.splitlines() if line.startswith('TAKEN_PICKUP ')]
    report['taken_pickups']=taken
    retired=words(monitor,symbol('rf_scene_defeated_actors'),18434)
    level_count,actor_count=retired[:2];assert level_count<=128 and actor_count<=2048
    retired_bytes=struct.pack('<18434I',*retired);defeated=[];actor_vitals=[];actor_mission=[]
    for index in range(actor_count):
     level_index,uid,dead=struct.unpack_from('<III',retired_bytes,8200+index*12)
     assert level_index<level_count and dead<=1
     name=retired_bytes[8+level_index*64:8+(level_index+1)*64].split(b'\0')[0].decode('ascii')
     valid,health,armor=struct.unpack_from('<III',retired_bytes,32776+index*12)
     assert valid<=1
     if valid:actor_vitals.append(f'ACTOR_VITALS {name} {uid} {health} {armor}')
     affiliation,flags=struct.unpack_from('<II',retired_bytes,57352+index*8)
     assert flags & ~0x4004 == 0
     if valid:actor_mission.append(f'ACTOR_MISSION_STATE {name} {uid} {affiliation} {flags}')
     if dead:
      name=retired_bytes[8+level_index*64:8+(level_index+1)*64].split(b'\0')[0].decode('ascii')
      defeated.append(f'DEFEATED_ACTOR {name} {uid}')
    assert defeated==[line for line in pc.stdout.splitlines() if line.startswith('DEFEATED_ACTOR ')]
    report['defeated_actors']=defeated
    assert actor_vitals==[line for line in pc.stdout.splitlines() if line.startswith('ACTOR_VITALS ')]
    report['actor_vitals']=actor_vitals
    assert actor_mission==[line for line in pc.stdout.splitlines() if line.startswith('ACTOR_MISSION_STATE ')]
    report['actor_mission']=actor_mission
    report['actor_retirement']=words(monitor,symbol('rf_scene_actor_retirement'),4)
    assert report['actor_retirement']==expected('ACTOR_RETIREMENT')
    report['script_grants']=words(monitor,symbol('rf_scene_script_grants'),8)
    assert report['script_grants']==expected('SCRIPT_GRANTS')
    report['actor_revisit']=words(monitor,symbol('rf_scene_actor_revisit'),8)
    assert report['actor_revisit']==expected('ACTOR_REVISIT')
    report['level_transitions']=words(monitor,symbol('rf_xbox_level_transitions'),4)
    assert report['level_transitions'][:3]==([len(pc_transitions),int(pc_transitions[-1][2]),int(pc_transitions[-1][3])] if pc_transitions else [0,0,0])
    replay_state=words(monitor,symbol('rf_player_replay_diagnostic'),4);assert replay_state==[0,frames,frames,0],replay_state
    assert d[37]==frames and d[46]==3145728
    peak=max(s[36]*56 for s in report['samples']);report['sampled_gpu_mesh_peak_bytes']=peak
    if args.require_wide:assert peak>1048576 and expected('ACTOR_FOLLOW_SUMMARY')[2]>1048576
    if args.capture:
     from PIL import Image
     monitor.command('stop');capture=run/'framebuffer.bin'
     monitor.command('human-monitor-command',{'command-line':f'pmemsave 0x{d[32]&0x03ffffff:x} {d[35]*d[34]} "{capture.as_posix()}"'})
     Image.frombytes('RGB',(d[33],d[34]),capture.read_bytes(),'raw','BGRX',d[35],1).save(run/'framebuffer.png')
     report['capture']='Native guest framebuffer for renderer validation'
     if args.campaign_spawn:
      native=Image.open(run/'framebuffer.png');reference=Image.open(run/'pc-final.ppm')
      points=[(312,240),(30,448),(150,448),(30,463),(100,463),(150,463),(570,418),(578,430),(590,424)]+[(int(479+i*132/report['pistol_rules'][0]),448) for i in range(report['pistol_rules'][0])]
      if report['combat'][6]:points.extend([(480,459),(600,459)])
      report['combat_hud_pixels']=[dict(point=q,xbox=native.getpixel(q),pc=reference.getpixel(q)) for q in points]
      assert all(max(abs(a-b) for a,b in zip(v['xbox'],v['pc']))<=2 for v in report['combat_hud_pixels']),report['combat_hud_pixels']

    if args.audio_capture:
     device_audio=words(monitor,symbol('rf_xbox_audio_diagnostic'),12)
     report['device_audio']=device_audio
     assert device_audio[0]==0 and device_audio[3]==0 and device_audio[11]==0,device_audio
     assert device_audio[1]==expected('LIVE_AUDIO')[4] and device_audio[4]==1 and device_audio[5]>0,device_audio
     import struct
     pcm_words=words(monitor,symbol('rf_xbox_audio_snapshot'),2048)
     (run/'apu-dma-snapshot.bin').write_bytes(struct.pack('<2048I',*pcm_words))
    report['result']='PASS';break
   time.sleep(.5)
  else:raise RuntimeError('Replay did not complete before deadline')
except Exception as exc:
 report['error']=repr(exc)
 if monitor:
  try:
   report['lightmap_regeneration_failure']=words(monitor,symbol('rf_scene_lightmap_regeneration'),8)
   report['lightmap_updates_failure']=words(monitor,symbol('rf_scene_lightmap_updates'),8)
  except Exception as telemetry_error:report['lighting_telemetry_error']=repr(telemetry_error)
 if monitor:
  try:(run/'guest-memory-failure.json').write_text(json.dumps(snapshot(monitor,mapping),indent=2))
  except Exception as snapshot_error:report['snapshot_error']=repr(snapshot_error)
 raise
finally:
 if monitor:
  try:monitor.command('quit')
  except Exception:pass
  try:monitor.close()
  except OSError:pass
 if process:
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
 try:
  if args.audio_capture and report['result']=='PASS':
   import array
   pcm=(run/'apu-dma-snapshot.bin').read_bytes();assert len(pcm)==8192
   values=array.array('h');values.frombytes(pcm)
   report['device_dma']=dict(bytes=len(pcm),nonzero_samples=sum(v!=0 for v in values),sha256=hashlib.sha256(pcm).hexdigest(),scope='Guest DSP DMA ring snapshot, not a linear recording or host audibility proof')
   assert report['device_dma']['nonzero_samples']>0,'DSP output contains only silence'
 except Exception as capture_error:
  report['result']='FAIL';report['capture_error']=repr(capture_error)
 try:
  if saved_exit_start is None:exit_start_file.unlink(missing_ok=True)
  else:exit_start_file.write_bytes(saved_exit_start)
  if saved_return is None:return_file.unlink(missing_ok=True)
  else:return_file.write_bytes(saved_return)
  if saved_follow is None:follow_file.unlink(missing_ok=True)
  else:follow_file.write_bytes(saved_follow)
  if saved_setup is None:setup_file.unlink(missing_ok=True)
  else:setup_file.write_bytes(saved_setup)
  if saved_goto is None:goto_file.unlink(missing_ok=True)
  else:goto_file.write_bytes(saved_goto)
  if saved_watch is None:watch_file.unlink(missing_ok=True)
  else:watch_file.write_bytes(saved_watch)
  if saved_goal is None:goal_file.unlink(missing_ok=True)
  else:goal_file.write_bytes(saved_goal)
  if saved_exit is None:exit_file.unlink(missing_ok=True)
  else:exit_file.write_bytes(saved_exit)
  if saved_volume is None:volume_flag.unlink(missing_ok=True)
  else:volume_flag.write_bytes(saved_volume)
  if saved_regen is None:regen_flag.unlink(missing_ok=True)
  else:regen_flag.write_bytes(saved_regen)
  if saved_loss is None:loss_flag.unlink(missing_ok=True)
  else:loss_flag.write_bytes(saved_loss)
  if saved_pair is None:pair_flag.unlink(missing_ok=True)
  else:pair_flag.write_bytes(saved_pair)
  if saved_death is None:death_flag.unlink(missing_ok=True)
  else:death_flag.write_bytes(saved_death)
  if saved_damage is None:damage_file.unlink(missing_ok=True)
  else:damage_file.write_bytes(saved_damage)
  if saved_item is None:item_file.unlink(missing_ok=True)
  else:item_file.write_bytes(saved_item)
  if saved_actor is None:actor_file.unlink(missing_ok=True)
  else:actor_file.write_bytes(saved_actor)
  if saved_force is None:force_file.unlink(missing_ok=True)
  else:force_file.write_bytes(saved_force)
  if saved_audio is None:audio_flag.unlink(missing_ok=True)
  else:audio_flag.write_bytes(saved_audio)
  if saved_steps is None:step_file.unlink(missing_ok=True)
  else:step_file.write_bytes(saved_steps)
  if saved_selection is None:selection_file.unlink(missing_ok=True)
  else:selection_file.write_bytes(saved_selection)
  if saved_climb is None:climb_flag.unlink(missing_ok=True)
  else:climb_flag.write_bytes(saved_climb)
  if saved is None:replay.unlink(missing_ok=True)
  else:replay.write_bytes(saved)
  if saved_lift is None:lift_flag.unlink(missing_ok=True)
  else:lift_flag.write_bytes(saved_lift)
  if saved_door is None:door_flag.unlink(missing_ok=True)
  else:door_flag.write_bytes(saved_door)
  if saved_spawn is None:spawn_flag.unlink(missing_ok=True)
  else:spawn_flag.write_bytes(saved_spawn)
  build()
 except Exception as restore_error:
  report['result']='FAIL';report['restore_error']=repr(restore_error);raise
 finally:(run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
 if report['result']!='PASS':raise RuntimeError(report.get('capture_error',report.get('error','Replay failed')))
