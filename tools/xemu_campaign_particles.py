"""Stock-64-MiB authored Particle_State replay; no host input or desktop capture."""
import datetime,hashlib,json,os,re,shutil,socket,subprocess,time,struct
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu')
run=root/'artifacts/xemu'/('campaign-particles-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
flag=root/'build/xbox/disc/campaign-particle-test.flag';saved=flag.read_bytes() if flag.exists() else None
extra=root/'build/xbox/disc/levels2.vpp';extra_created=not extra.exists()
process=monitor=None;report={'result':'FAIL','scope':'Six installed levels and 13 authored Particle_State events through native owned loading, actor resolution, contact polling, registered trigger activation and scheduling. Controlled trigger/preconditions; no natural campaign trigger or rendering claim.'}

def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh','--repack'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True,stdout=subprocess.DEVNULL)
try:
 if extra_created:shutil.copyfile(root/'Installed_Game/levels2.vpp',extra)
 flag.write_bytes(b'1');build();mapping=(root/'build/xbox/main.map').read_text()
 address=int(re.search(r'_rf_campaign_particle_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
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
  deadline=time.monotonic()+120
  while time.monotonic()<deadline:
   if process.poll() is not None:raise RuntimeError(f'XEMU exited {process.returncode}')
   if monitor is None:
    try:monitor=Monitor(port)
    except OSError:time.sleep(.5);continue
    report['memory']=monitor.command('query-memory-size-summary');assert report['memory']['base-memory']==64*1024*1024
   try:state=words(monitor,address,16)
   except RuntimeError as exc:
    if 'received 0' not in str(exc):raise
    time.sleep(.5);continue
   report['state']=state
   if state[0]==0x52464350:
    if state[1]&0x80000000:raise RuntimeError(f'Guest failure {state}')
    if state[1]==2:break
   time.sleep(.5)
  else:raise TimeoutError(f'Campaign particle test timeout: {report.get("state")}')
  assert state[2]==6 and 0<state[3]<8192,state
  address_text=int(re.search(r'_rf_campaign_particle_text\s+([0-9a-fA-F]+)',mapping)[1],16)
  values=words(monitor,address_text,(state[3]+3)//4)
  actual=struct.pack('<'+'I'*len(values),*values)[:state[3]].decode('ascii')
  levels=[('L4S1a.rfl',1),('L4S1b.rfl',1),('L5S2.rfl',1),('L7S4.rfl',2),('L9S3.rfl',2),('L18S3.rfl',3)]
  expected=''
  for name,pack in levels:
   expected+='LEVEL '+name+'\n'+subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--campaign-particle-events',str(root/'Installed_Game'/f'levels{pack}.vpp'),name,str(root/'Installed_Game'),'1048576'],text=True)
  report['actual']=actual;report['expected']=expected;report['pages_after_levels']=state[6:12]
  assert actual==expected,(actual,expected)
  assert sum(line.startswith('EVENT ') for line in actual.splitlines())==13
  assert actual.splitlines().count('TRIGGER 1 64 2 150 123')==13
  assert actual.splitlines().count('CONTACT 80 -1 100 100 -1')==13
  assert all(p>0 for p in state[4:12]) and state[5]>=state[4],state
  report['result']='PASS'
finally:
 if monitor:
  try:monitor.command('quit')
  except (OSError,RuntimeError):pass
  monitor.close()
 if process:
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.kill();process.wait()
 if saved is None:flag.unlink(missing_ok=True)
 else:flag.write_bytes(saved)
 if extra_created:extra.unlink(missing_ok=True)
 build();(run/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(run/'report.json',{'result':report['result'],'state':report.get('state'),'pages_after_levels':report.get('pages_after_levels')},flush=True)
