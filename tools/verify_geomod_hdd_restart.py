"""Disposable stock64MiB two-launch HDD checkpoint proof; default is preflight only.

Required guest hooks (not supplied by this tool): dev-room + geomod-hdd-save.flag
selects slots before gameplay and stores/flushes RFDS at successful end;
geomod-hdd-load.flag selects/restores HDD before first frame. Both phases also
honor geomod-checkpoint-out.flag for bounded QMP RFDS export. Preserve storage
phase5(write)/3(read), successful status, generation/slot until QMP inspection.
No staged geomod-checkpoint.bin is present. Close alias after inspection or
retain operation diagnostics separately if scene closes it before inspection.

This verifies exact DEV destruction persistence, not visuals/full campaign or
power-cut durability. Never controls host input or takes screenshots.
"""
import argparse,datetime,hashlib,json,os,re,shutil,socket,struct,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
ROOT=Path(__file__).resolve().parents[1]
LIMIT=110524
NAMES=('player-replay.bin','player-control-frames.txt','audio-output.flag','particle-step-fixtures.bin',
 'renderer-cull-off.flag','renderer-cull-on.flag','renderer-batch-off.flag','renderer-world-off.flag',
 'dev-room.flag','terrain-test-light.flag','shallow-fixture.flag','geomod-checkpoint.bin',
 'geomod-checkpoint-out.flag','geomod-hdd-load.flag','geomod-hdd-save.flag','ripple-test.flag','water-test.flag')

def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()

def replay(path):
 b=path.read_bytes();size={b'RFI2':28,b'RFI3':32,b'RFI4':40,b'RFI5':44,b'RFI6':48}.get(b[:4])
 if not size or b[4:8]!=struct.pack('<I',size) or (len(b)-8)%size:raise ValueError('Malformed replay '+str(path))
 n=(len(b)-8)//size
 if not 32<=n<=60000:raise ValueError('Require32..60000 replay frames')
 return b,n

def symbol(mapping,name):
 m=re.search('_'+re.escape(name)+r'\s+([0-9a-fA-F]+)',mapping)
 if not m:raise ValueError('Missing linked guest hook '+name+'; integrate native HDD scene flags before --run')
 return int(m[1],16)

def owned_copy(base,destination):
 # QCOW2 backing chains can involve a user's HDD. Require a standalone source;
 # no flatten/convert command is inferred and no existing destination is reused.
 with base.open('rb') as f:header=f.read(24)
 if len(header)!=24 or header[:4]!=b'QFI\xfb' or struct.unpack_from('>I',header,4)[0] not in (2,3):raise ValueError('Require QCOW2 source')
 if struct.unpack_from('>Q',header,8)[0] or struct.unpack_from('>I',header,16)[0]:raise ValueError('Source has backing chain; supply an independently flattened harness base first')
 if destination.exists() or destination.parent.resolve().parent!= (ROOT/'artifacts/geomod-hdd').resolve():raise ValueError('Destination is not new owned run HDD')
 shutil.copyfile(base,destination)
 if digest(base)!=digest(destination):raise RuntimeError('Private HDD copy differs')

def iso_stage(run,phase,inputs,shallow,packer):
 disc=ROOT/'build/xbox/disc';names=set(NAMES)|{p.name for p in disc.glob('campaign-*') if p.is_file()}
 names.update(('campaign-spawn.flag','campaign-level.bin'))
 saved={n:(disc/n).read_bytes() if (disc/n).exists() else None for n in names}
 (run/(phase+'-disc-restore.json')).write_text(json.dumps({n:b.hex() if b is not None else None for n,b in saved.items()},indent=2))
 try:
  for n in names:(disc/n).unlink(missing_ok=True)
  for n in ('campaign-spawn.flag','dev-room.flag','geomod-checkpoint-out.flag','geomod-hdd-'+('save' if phase=='write' else 'load')+'.flag'):(disc/n).write_bytes(b'')
  (disc/'campaign-level.bin').write_bytes(b'levelsm.vpp'.ljust(64,b'\0')+b'glass_house.rfl'.ljust(64,b'\0'))
  (disc/'player-replay.bin').write_bytes(inputs)
  if shallow:(disc/'shallow-fixture.flag').write_bytes(str(shallow).encode())
  iso=run/(phase+'.iso')
  with (run/(phase+'-pack.log')).open('wb') as log:subprocess.run([str(packer),'-c',str(disc),str(iso)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
  return iso
 finally:
  for n,b in saved.items():
   if b is None:(disc/n).unlink(missing_ok=True)
   else:(disc/n).write_bytes(b)
  if any(((disc/n).read_bytes() if (disc/n).exists() else None)!=b for n,b in saved.items()):raise RuntimeError('Disc restoration mismatch')

def launch(run,phase,iso,hdd,mapping,frames,seconds,emulator):
 require_no_project_xemu(ROOT)
 if hdd.resolve()!=run.resolve()/'save-test.qcow2' or not hdd.is_file():raise RuntimeError('Refusing non-owned writable HDD')
 config=run/(phase+'.toml')
 config.write_text(f"""[general]
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
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{hdd.as_posix()}'
dvd_path = '{iso.as_posix()}'
""")
 with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
 command=[str(emulator/'xemu.exe'),'-config_path',str(config),'-m','64','-display','xemu','-audio','none','-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
 startup=None
 if os.name=='nt':startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 monitor=process=None;result=dict(phase=phase,command=command,snapshot=False)
 try:
  with (run/(phase+'-stdout.log')).open('wb') as out,(run/(phase+'-stderr.log')).open('wb') as err:
   require_no_project_xemu(ROOT)
   process=subprocess.Popen(command,cwd=run,env=dict(os.environ,SDL_AUDIO_DRIVER='dummy'),stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
   result['pid']=process.pid;(run/(phase+'-live.json')).write_text(json.dumps(dict(pid=process.pid,port=port)))
   deadline=time.monotonic()+seconds;last=None
   while time.monotonic()<deadline:
    if process.poll() is not None:raise RuntimeError('Owned XEMU exited before checkpoint')
    if monitor is None:
     try:monitor=Monitor(port)
     except OSError:time.sleep(.5);continue
     result['memory']=monitor.command('query-memory-size-summary')
     if result['memory']!={'base-memory':64*1024*1024,'plugged-memory':0}:raise RuntimeError('Guest is not stock64MiB')
    try:d=words(monitor,symbol(mapping,'rf_diagnostic'),58)
    except RuntimeError as exc:
     if 'received 0' not in str(exc):raise
     time.sleep(.5);continue
    if d[0]!=0x52464447:time.sleep(.5);continue
    if d[2]&0x80000000:raise RuntimeError(f'Guest failure {d[2]:08x}')
    if (d[2],d[37]//120)!=last:print(phase,'stage',d[2],'frame',d[37],flush=True);last=(d[2],d[37]//120)
    if d[2]==5:break
    time.sleep(.5)
   else:raise TimeoutError('Guest checkpoint deadline')
   monitor.command('stop')
   result['diagnostic']=d
   if d[37]!=frames:raise RuntimeError('Replay frame count differs')
   state=words(monitor,symbol(mapping,'rf_xbox_checkpoint_storage_state'),8);result['storage']=state
   if state[0]!=(5 if phase=='write' else 3) or state[1] or not state[4] or state[5]>1:raise RuntimeError('Storage operation not successful: '+str(state))
   if phase=='write' and not state[7]&4:raise RuntimeError('Native flush not confirmed')
   check=words(monitor,symbol(mapping,'rf_scene_geomod_checkpoint_state'),4);memory=words(monitor,symbol(mapping,'rf_scene_geomod_checkpoint_memory'),2)
   pointer=words(monitor,symbol(mapping,'rf_scene_geomod_checkpoint_data'),1)[0]
   if check[0] or check[3]!=1 or not 288<=check[1]<=LIMIT or not pointer or not 0<memory[0]<=memory[1]<=LIMIT:raise RuntimeError('Invalid RFDS export/budget')
   data=bytearray()
   for at in range(0,check[1],4096):
    count=(min(4096,check[1]-at)+3)//4;data.extend(struct.pack('<'+'I'*count,*words(monitor,pointer+at,count)))
   data=bytes(data[:check[1]]);fnv=2166136261
   for v in data:fnv=((fnv^v)*16777619)&0xffffffff
   if fnv!=check[2]:raise RuntimeError('Export FNV mismatch')
   (run/(phase+'.rfds')).write_bytes(data);result.update(checkpoint=check,checkpoint_memory=memory,sha256=hashlib.sha256(data).hexdigest())
   return result,data
 finally:
  if monitor:
   try:monitor.command('quit')
   except (OSError,RuntimeError):pass
   monitor.close()
  if process:
   try:process.wait(timeout=10)
   except subprocess.TimeoutExpired:
    process.terminate();process.wait(timeout=10);result['forced_termination']=True
  (run/(phase+'-result.json')).write_text(json.dumps(result,indent=2))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',action='store_true',help='Explicitly execute two launches; default only checks inputs/hooks')
 p.add_argument('--write-input',type=Path,required=True);p.add_argument('--read-input',type=Path,required=True);p.add_argument('--expected',type=Path,required=True,help='Verified PC RFDS after write; read replay must not edit terrain')
 p.add_argument('--shallow',type=int,choices=[0,1,2,3],default=0);p.add_argument('--seconds',type=int,default=300)
 args=p.parse_args()
 if not 30<=args.seconds<=3600:p.error('Require30..3600 seconds')
 write,nwrite=replay(args.write_input);read,nread=replay(args.read_input);expected=args.expected.read_bytes()
 if not 288<=len(expected)<=LIMIT or expected[:4]!=b'RFDS':p.error('Invalid expected RFDS')
 mapping=(ROOT/'build/xbox/main.map').read_text()
 for name in ('rf_diagnostic','rf_xbox_checkpoint_storage_state','rf_scene_geomod_checkpoint_state','rf_scene_geomod_checkpoint_data','rf_scene_geomod_checkpoint_memory'):symbol(mapping,name)
 base=ROOT/'local/xemu-harness/pacing-base.qcow2';emulator=Path('C:/Games/Emulators/Xemu');packer=Path('C:/nxdk/tools/extract-xiso/build/extract-xiso.exe')
 for path in (base,packer,emulator/'xemu.exe',emulator/'eeprom.bin',ROOT/'build/xbox/disc/default.xbe'):
  if not path.is_file():raise FileNotFoundError(path)
 if not args.run:print('Preflight inputs/symbols available. No launch, staging or HDD copy performed. Scene flag semantics still require source review.');return
 require_no_project_xemu(ROOT)
 run=ROOT/'artifacts/geomod-hdd'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f');run.mkdir(parents=True)
 report=dict(result='FAIL',scope='Exact DEV RFDS restart persistence only; no visual/fullsave/power-loss claim',base=str(base),base_sha256=digest(base),expected_sha256=hashlib.sha256(expected).hexdigest(),xbe_sha256=digest(ROOT/'build/xbox/disc/default.xbe'))
 try:
  hdd=run/'save-test.qcow2';owned_copy(base,hdd);shutil.copyfile(emulator/'eeprom.bin',run/'eeprom.bin');(run/'main.map').write_text(mapping)
  for phase,payload,frames in [('write',write,nwrite),('read',read,nread)]:
   require_no_project_xemu(ROOT)
   if digest(ROOT/'build/xbox/disc/default.xbe')!=report['xbe_sha256']:raise RuntimeError('XBE changed during test')
   iso=iso_stage(run,phase,payload,args.shallow,packer);result,data=launch(run,phase,iso,hdd,mapping,frames,args.seconds,emulator);report[phase]=result
   if result.get('forced_termination'):raise RuntimeError('XEMU required termination; clean restart persistence not established')
   if data!=expected:raise RuntimeError(phase+' RFDS differs from PC baseline')
  if report['write']['storage'][4:7]!=report['read']['storage'][4:7]:raise RuntimeError('Read did not select written generation/slot/bytes')
  report['result']='PASS'
 finally:
  report['base_unchanged']=digest(base)==report['base_sha256']
  if not report['base_unchanged']:report['result']='FAIL'
  (run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
 if report['result']!='PASS':raise RuntimeError('HDD source changed')
if __name__=='__main__':main()
