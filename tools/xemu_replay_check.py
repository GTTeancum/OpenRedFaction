"""Replay a bounded command file inside Xbox and compare native guest state to PC."""
import argparse,datetime,hashlib,json,os,re,shutil,socket,subprocess,sys,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words,snapshot
p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('--seconds',type=int,default=180);p.add_argument('--require-wide',action='store_true');p.add_argument('--campaign-spawn',action='store_true');p.add_argument('--approach',action='store_true',help='Stage outside the first L1S2 climb region');p.add_argument('--climb',action='store_true',help='Staged L1S2 first-region campaign replay');p.add_argument('--capture',action='store_true',help='Native guest framebuffer for renderer validation');p.add_argument('--level',help='Authored campaign level, without climb staging');p.add_argument('--archive',default='levels1.vpp',choices=['levels1.vpp','levels2.vpp','levels3.vpp','levelsm.vpp']);args=p.parse_args()
if args.approach:args.climb=True
if args.climb:args.campaign_spawn=True
if args.level:
 if args.climb or not re.fullmatch(r'[A-Za-z0-9_-]+\.rfl',args.level) or len(args.level)>63:p.error('Choose a plain level name, without climb staging')
 args.campaign_spawn=True
elif args.archive!='levels1.vpp':p.error('--archive requires --level')
replay_env=dict(os.environ)
for key in ('RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START'):replay_env.pop(key,None)
replay_env.update(RF_REPLAY_LEVEL=args.level or ('L1S2.rfl' if args.climb else 'L1S1.rfl'),RF_REPLAY_ARCHIVE=args.archive)
if args.climb:replay_env['RF_REPLAY_REGION_START']='2' if args.approach else '1'

root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu');payload=args.input.read_bytes()
record_size=28 if payload[:4]==b'RFI2' else 24
offset=8 if record_size==28 else 0
if offset and payload[4:8]!=(28).to_bytes(4,'little'):raise ValueError('Invalid RFI2 record size')
if len(payload)<=offset or (len(payload)-offset)%record_size or (len(payload)-offset)>60000*record_size:raise ValueError('Expected 1..60000 input records')
frames=(len(payload)-offset)//record_size;run=root/'artifacts/xemu'/('replay-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
source=run/'inputs.bin';source.write_bytes(payload)
pc=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--spawn-replay' if args.campaign_spawn else '--replay',str(root/'Installed_Game'),str(source),str(run/'pc-final.ppm')],capture_output=True,text=True,check=True,env=replay_env)
(run/'pc-reference.txt').write_text(pc.stdout)
def expected(label):return list(map(int,next(x for x in pc.stdout.splitlines() if x.startswith(label+' ')).split()[1:]))
if args.campaign_spawn and not args.climb:
 starts=json.loads((root/'artifacts/player-start-verification.json').read_text())
 look=json.loads((root/'artifacts/player-spawn-look.json').read_text())
 original='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
 assert starts['original_sha256']==look['original_sha256']==original
 start=next(c for c in starts['levels'] if c['file'].lower()==(args.level or 'L1S1.rfl').lower())
 angles=next(c for c in look['cases'] if c['file'].lower()==(args.level or 'L1S1.rfl').lower())
 assert expected('PLAYER_SPAWN')==[1]+start['transform_words']+angles['body_words']+angles['eye_words']
if args.campaign_spawn:assert expected('PC_PLAY_BODY')[68]&0x80, 'Campaign player physics flag missing'
hdd=root/'local/xemu-harness/pacing-base.qcow2'
if not hdd.exists():raise ValueError('Run the pacing harness once to prepare its separate HDD base')
assert (root/'build/xbox/disc/player-control.flag').exists()
replay=root/'build/xbox/disc/player-replay.bin';saved=replay.read_bytes() if replay.exists() else None
spawn_flag=root/'build/xbox/disc/campaign-spawn.flag';saved_spawn=spawn_flag.read_bytes() if spawn_flag.exists() else None
climb_flag=root/'build/xbox/disc/campaign-climb.flag';saved_climb=climb_flag.read_bytes() if climb_flag.exists() else None
selection_file=root/'build/xbox/disc/campaign-level.bin';saved_selection=selection_file.read_bytes() if selection_file.exists() else None
process=monitor=None;report={'result':'FAIL','level':args.level or ('L1S2.rfl' if args.climb else 'L1S1.rfl'),'archive':args.archive,'frames':frames,'input_sha256':hashlib.sha256(payload).hexdigest(),'pc_sha256':hashlib.sha256((root/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(),'samples':[],'scope':'Guest command replay, submission counts, CPU world/camera hashes and final body; optional native framebuffer capture, no PS2 parity claim.'}
def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True)
try:
 if args.level:
  archive_source=root/'Installed_Game'/args.archive;archive_target=root/'build/xbox/disc'/args.archive
  archive_sha=hashlib.sha256(archive_source.read_bytes()).hexdigest()
  if not archive_target.exists():shutil.copyfile(archive_source,archive_target)
  if hashlib.sha256(archive_target.read_bytes()).hexdigest()!=archive_sha:raise ValueError('Staged archive differs from installed source')
  report['archive_sha256']=archive_sha
 if args.level:selection_file.write_bytes(args.archive.encode().ljust(64,b'\0')+args.level.encode().ljust(64,b'\0'))
 else:selection_file.unlink(missing_ok=True)
 if args.climb:climb_flag.write_bytes(b'2' if args.approach else b'')
 else:climb_flag.unlink(missing_ok=True)
 replay.write_bytes(payload)
 if args.campaign_spawn:spawn_flag.write_bytes(b'')
 elif spawn_flag.exists():spawn_flag.unlink()
 build();mapping=(root/'build/xbox/main.map').read_text()
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
  process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
  deadline=time.monotonic()+args.seconds;last=-1
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
   if d[2]&0x80000000:raise RuntimeError(f'Guest error {d[2]:08x}')
   if d[37]//60!=last:last=d[37]//60;print('Submitted',d[37],'frames',flush=True)
   if d[2]==5:
    final=snapshot(monitor,mapping);(run/'guest-memory-final.json').write_text(json.dumps(final,indent=2))
    for name,label,count in [('rf_scene_actor_follow_summary','ACTOR_FOLLOW_SUMMARY',5),('rf_scene_player_input_frames','ACTOR_PLAYER_INPUT',448),('scene_actor_body','PC_PLAY_BODY',77)]:
     got=words(monitor,symbol(name),count);assert got==expected(label),name;report[name]=got
    if args.campaign_spawn:
     owned=words(monitor,symbol('rf_scene_campaign_events'),3)
     assert owned[0]==expected('CAMPAIGN_EVENTS')[0] and owned[1]<=1024*1024
     report['campaign_events']=owned
     triggers=words(monitor,symbol('rf_scene_campaign_triggers'),2)
     assert triggers[0]==expected('CAMPAIGN_TRIGGERS')[0] and triggers[1]<=1024*1024
     report['campaign_triggers']=triggers
     links=words(monitor,symbol('rf_scene_campaign_links'),4)
     assert links==expected('CAMPAIGN_LINKS')
     report['campaign_links']=links
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
    replay_state=words(monitor,symbol('rf_player_replay_diagnostic'),4);assert replay_state==[0,frames,frames,0],replay_state
    assert d[37]==frames and d[46]==2097152
    peak=max(s[36]*56 for s in report['samples']);report['sampled_gpu_mesh_peak_bytes']=peak
    if args.require_wide:assert peak>1048576 and expected('ACTOR_FOLLOW_SUMMARY')[2]>1048576
    if args.capture:
     from PIL import Image
     monitor.command('stop');capture=run/'framebuffer.bin'
     monitor.command('human-monitor-command',{'command-line':f'pmemsave 0x{d[32]&0x03ffffff:x} {d[35]*d[34]} "{capture.as_posix()}"'})
     Image.frombytes('RGB',(d[33],d[34]),capture.read_bytes(),'raw','BGRX',d[35],1).save(run/'framebuffer.png')
     report['capture']='Native guest framebuffer for renderer validation'
    report['result']='PASS';break
   time.sleep(.5)
  else:raise RuntimeError('Replay did not complete before deadline')
except Exception as exc:
 report['error']=repr(exc)
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
  if saved_selection is None:selection_file.unlink(missing_ok=True)
  else:selection_file.write_bytes(saved_selection)
  if saved_climb is None:climb_flag.unlink(missing_ok=True)
  else:climb_flag.write_bytes(saved_climb)
  if saved is None:replay.unlink(missing_ok=True)
  else:replay.write_bytes(saved)
  if saved_spawn is None:spawn_flag.unlink(missing_ok=True)
  else:spawn_flag.write_bytes(saved_spawn)
  build()
 except Exception as restore_error:
  report['result']='FAIL';report['restore_error']=repr(restore_error);raise
 finally:(run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
