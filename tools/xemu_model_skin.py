"""Isolated stock-64-MiB XEMU skeletal model collision tests; no host input or desktop capture."""
import datetime,hashlib,json,os,re,shutil,socket,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu')
run=root/'artifacts/xemu'/('model-skin-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
flag=root/'build/xbox/disc/model-skin-test.bin';saved=flag.read_bytes() if flag.exists() else None
process=monitor=None;report={'result':'FAIL','scope':'Native stock64MiB archive-loaded owned skeletal collision resources versus original-game answers and PC:95 models/170 LODs/4080 queries; prepared synthetic transforms. No live scene residency or visual claim.'}

def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh','--repack'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True,stdout=subprocess.DEVNULL)
try:
 flag.write_bytes((root/'artifacts/model-skin-authored-trace.bin').read_bytes());build();mapping=(root/'build/xbox/main.map').read_text()
 address=int(re.search(r'_rf_model_skin_test\s+([0-9a-fA-F]+)',mapping)[1],16)
 report['xbe_sha256']=hashlib.sha256((root/'build/xbox/disc/default.xbe').read_bytes()).hexdigest()
 report['map_sha256']=hashlib.sha256(mapping.encode()).hexdigest()
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
hdd_path = '{root.as_posix()}/local/xemu-harness/pacing-base.qcow2'
dvd_path = '{root.as_posix()}/build/xbox/redfaction-diagnostic.iso'
''')
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 command=[str(emulator/'xemu.exe'),'-config_path',str(config),'-m','64','-snapshot','-display','xemu','-audio','none','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
 with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
  process=subprocess.Popen(command,cwd=run,stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
  deadline=time.monotonic()+360
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError(f'XEMU exited {process.returncode}')
   if monitor is None:
    try:monitor=Monitor(port)
    except OSError:time.sleep(.5);continue
    report['memory']=monitor.command('query-memory-size-summary');assert report['memory']['base-memory']==64*1024*1024
   try:state=words(monitor,address,8)
   except RuntimeError as exc:
    if 'received 0' not in str(exc):raise
    time.sleep(.5);continue
   report['state']=state
   if state[0]==0x52465354:
    if state[1]&0x80000000:raise RuntimeError(f'Guest failure {state}')
    if state[1]==2:break
   time.sleep(.5)
  else:raise TimeoutError(f'Model collision timeout: {report.get("state")}')
  expected=[int(v) for v in subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),'--skin-fixture',str(root/'artifacts/model-skin-authored-trace.bin'),str(root/'Installed_Game')+'/'],text=True).split()]
  report['pc_state']=expected
  assert state==expected and state[2:5]==[95,4080,2674],(state,expected)
  report['result']='PASS'
finally:
 if monitor:
  try:monitor.command('quit')
  except (OSError,RuntimeError):pass
  try:monitor.close()
  except (OSError,RuntimeError):pass
 if process:
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
 if saved is None:flag.unlink(missing_ok=True)
 else:flag.write_bytes(saved)
 build();(run/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(run/'report.json',{'result':report['result'],'state':report.get('state')},flush=True)
