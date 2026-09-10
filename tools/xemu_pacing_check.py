"""Read pacing progress in an isolated stock-64-MiB XEMU session, without input/capture."""
import datetime,hashlib,json,os,re,shutil,socket,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words,snapshot
root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu')
with socket.socket() as reservation:
 reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
run=root/'artifacts/xemu'/('pacing-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
run.mkdir(parents=True);mapping=(root/'build/xbox/main.map').read_text()
def symbol(name):return int(re.search('_'+name+r'\s+([0-9a-fA-F]+)',mapping)[1],16)
assert (root/'build/xbox/disc/player-control.flag').exists()
assert (root/'build/xbox/disc/player-control-frames.txt').read_text().strip()=='0'
eeprom=run/'eeprom.bin';shutil.copyfile(emulator/'eeprom.bin',eeprom)
config=run/'xemu.toml'
config.write_text(f'''[general]
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
hdd_path = '{emulator.as_posix()}/HDD/xbox_hdd.qcow2'
dvd_path = '{root.as_posix()}/build/xbox/redfaction-diagnostic.iso'
''')
command=[str(emulator/'xemu.exe'),'-config_path',str(config),'-m','64','-snapshot','-display','xemu','-audio','none','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
report={'result':'FAIL','scope':'Live pacing progress only; no framebuffer, original-game or neutral-pose parity claim. Non-atomic RAM samples.','command':command,'samples':[],'map_sha256':hashlib.sha256(mapping.encode()).hexdigest(),'xbe_sha256':hashlib.sha256((root/'build/xbox/disc/default.xbe').read_bytes()).hexdigest(),'iso_sha256':hashlib.sha256((root/'build/xbox/redfaction-diagnostic.iso').read_bytes()).hexdigest()}
process=monitor=None;started=None
try:
 with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
  startup=None
  if os.name=='nt':
   startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
  process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
  deadline=time.monotonic()+100
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError(f'XEMU exited {process.returncode}')
   if monitor is None:
    try:monitor=Monitor(port)
    except OSError:time.sleep(.5);continue
    report['memory']=monitor.command('query-memory-size-summary');assert report['memory']['base-memory']==64*1024*1024
   try:c=words(monitor,symbol('rf_player_frame_clock'),8)
   except RuntimeError as exc:
    if started is not None or 'received 0' not in str(exc):raise
    time.sleep(.5);continue
   if c[0]==1 and c[4]>0:
    sample={'host_seconds':time.monotonic(),'clock':c,'profile':words(monitor,symbol('rf_scene_profile'),32),'profile_stage':words(monitor,symbol('rf_scene_profile_stage'),2),'diagnostic':words(monitor,symbol('rf_diagnostic'),58)}
    report['samples'].append(sample);print('Pacing:',c,flush=True)
    if sample['diagnostic'][2]&0x80000000:raise RuntimeError(f"Guest diagnostic error {sample['diagnostic'][2]:08x}")
    if started is None:started=time.monotonic()
    if time.monotonic()-started>=20:
     first=report['samples'][0]['clock'];last=c
     assert last[4]>first[4]+60 and last[5]>first[5]
     assert last[4]>report['samples'][-3]['clock'][4], 'Pacing stalled at end of observation'
     assert last[5]<=last[4] and last[2]<=8000 and last[3]<=8
     assert sample['diagnostic'][37]>0
     elapsed=((last[1]-first[1])&0xffffffff)/1000
     report['observed_steps_per_guest_second']=(last[4]-first[4])/elapsed
     report['result']='PASS';break
   time.sleep(1)
  else:raise RuntimeError('No completed 20-second pacing observation within timeout')
except Exception as exc:
 report['error']=repr(exc)
 if monitor:
  (run/'guest-memory-final.json').write_text(json.dumps(snapshot(monitor,mapping),indent=2))
  report['status']=monitor.command('query-status')
  report['registers']=monitor.command('human-monitor-command',{'command-line':'info registers'})
 raise
finally:
 if monitor:
  try:monitor.command('quit')
  except Exception:pass
  monitor.close()
 if process:
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
 (run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
