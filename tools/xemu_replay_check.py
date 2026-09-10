"""Replay a bounded command file inside Xbox and compare native guest state to PC."""
import argparse,datetime,hashlib,json,os,re,shutil,socket,subprocess,sys,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words,snapshot
p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('--seconds',type=int,default=180);p.add_argument('--require-wide',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu');payload=args.input.read_bytes()
if not payload or len(payload)%24 or len(payload)>60000*24:raise ValueError('Expected 1..60000 input records')
frames=len(payload)//24;run=root/'artifacts/xemu'/('replay-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
source=run/'inputs.bin';source.write_bytes(payload)
pc=subprocess.run([str(root/'build/pc/Release/rf_pc_play.exe'),'--replay',str(root/'Installed_Game'),str(source),str(run/'pc-final.ppm')],capture_output=True,text=True,check=True)
(run/'pc-reference.txt').write_text(pc.stdout)
def expected(label):return list(map(int,next(x for x in pc.stdout.splitlines() if x.startswith(label+' ')).split()[1:]))
hdd=root/'local/xemu-harness/pacing-base.qcow2'
if not hdd.exists():raise ValueError('Run the pacing harness once to prepare its separate HDD base')
assert (root/'build/xbox/disc/player-control.flag').exists()
replay=root/'build/xbox/disc/player-replay.bin';saved=replay.read_bytes() if replay.exists() else None
process=monitor=None;report={'result':'FAIL','frames':frames,'input_sha256':hashlib.sha256(payload).hexdigest(),'pc_sha256':hashlib.sha256((root/'build/pc/Release/rf_pc_play.exe').read_bytes()).hexdigest(),'samples':[],'scope':'Guest command replay, submission counts, CPU world/camera hashes and final body; no framebuffer capture or PS2 parity claim.'}
def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True)
try:
 replay.write_bytes(payload);build();mapping=(root/'build/xbox/main.map').read_text()
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
    replay_state=words(monitor,symbol('rf_player_replay_diagnostic'),4);assert replay_state==[0,frames,frames,0],replay_state
    assert d[37]==frames and d[46]==2097152
    peak=max(s[36]*56 for s in report['samples']);report['sampled_gpu_mesh_peak_bytes']=peak
    if args.require_wide:assert peak>1048576 and expected('ACTOR_FOLLOW_SUMMARY')[2]>1048576
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
  if saved is None:replay.unlink(missing_ok=True)
  else:replay.write_bytes(saved)
  build()
 except Exception as restore_error:
  report['result']='FAIL';report['restore_error']=repr(restore_error);raise
 finally:(run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
