"""Explicit native semantic-invalid-newer fallback on a disposable HDD; default preflight.
Three bounded launches: seed absent slots; load+save via actual pure scene selector;
fresh load. Full RFSG hashes prove previous-slot preservation; exact RFDS/RFCP proves
restored DEV state. No screenshot, host input, or existing HDD modification.
"""
import argparse,datetime,hashlib,json,os,shutil,socket,struct,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
from xemu_session_guard import require_no_project_xemu
from verify_geomod_hdd_restart import ROOT,LIMIT,NAMES,digest,replay,symbol,owned_copy
from verify_geomod_hdd_fallback import fnv,envelope

def checkpoint_format(data,player_checkpoint):
 if not 288<=len(data)<=LIMIT or data[:4]!=(b'RFCP' if player_checkpoint else b'RFDS') or struct.unpack_from('<II',data,4)!=(1,len(data)):
  raise ValueError('Unexpected checkpoint format/version/length')
 if player_checkpoint:
  if len(data)<864 or struct.unpack_from('<8I',data,0)!=(int.from_bytes(b'RFCP','little'),1,len(data),0,1,544,len(data)-576,0):raise ValueError('Invalid RFCP envelope')
  if data[32:36]!=b'RFPL' or struct.unpack_from('<III',data,36)!=(1,544,0):raise ValueError('Invalid RFPL header')
  if any(data[100:112]) or any(data[560:576]):raise ValueError('Invalid RFPL reserved bytes')
  checkpoint_format(data[576:],False)
 return '.rfcp' if player_checkpoint else '.rfds'

def stage(run,phase,inputs,fixtures,shallow,packer,player_checkpoint=False):
 disc=ROOT/'build/xbox/disc';names=set(NAMES)|{p.name for p in disc.glob('campaign-*') if p.is_file()}
 names.update(('campaign-spawn.flag','campaign-level.bin','geomod-fallback-seed.flag','geomod-fallback-observe.flag','geomod-fallback0.rfsg','geomod-fallback1.rfsg'))
 saved={n:(disc/n).read_bytes() if (disc/n).exists() else None for n in names}
 (run/(phase+'-disc-restore.json')).write_text(json.dumps({n:b.hex() if b is not None else None for n,b in saved.items()},indent=2))
 try:
  for n in names:(disc/n).unlink(missing_ok=True)
  for n in ('campaign-spawn.flag','dev-room.flag'):(disc/n).write_bytes(b'')
  if player_checkpoint:(disc/'player-checkpoint.flag').write_bytes(b'')
  if phase=='seed':
   (disc/'geomod-fallback-seed.flag').write_bytes(b'')
   for i in range(2):(disc/f'geomod-fallback{i}.rfsg').write_bytes((fixtures/f'seed-slot{i}.rfsg').read_bytes())
  else:
   for n in ('geomod-checkpoint-out.flag','geomod-hdd-load.flag'):(disc/n).write_bytes(b'')
   if phase=='write':
    for n in ('geomod-hdd-save.flag','geomod-fallback-observe.flag'):(disc/n).write_bytes(b'')
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

def launch(run,phase,iso,hdd,mapping,frames,seconds,emulator,player_checkpoint=False):
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
    if d[2]&0x80000000:
     result['diagnostic']=d
     result['failure_state']={name:words(monitor,symbol(mapping,name),count) for name,count in
         [('rf_xbox_checkpoint_storage_state',8),('rf_xbox_checkpoint_fixture_seed_state',8),
          ('rf_xbox_checkpoint_fixture_copy_state',8),('rf_scene_geomod_checkpoint_state',4)]}
     raise RuntimeError(f'Guest failure {d[2]:08x}: '+str(result['failure_state']))
    if (d[2],d[37]//120)!=last:print(phase,'stage',d[2],'frame',d[37],flush=True);last=(d[2],d[37]//120)
    if d[2]==5:break
    time.sleep(.5)
   else:raise TimeoutError('Guest checkpoint deadline')
   monitor.command('stop')
   result['diagnostic']=d
   if d[37]!=frames:raise RuntimeError('Replay frame count differs')
   if phase=='seed':
    result['seed']=words(monitor,symbol(mapping,'rf_xbox_checkpoint_fixture_seed_state'),8)
    if result['seed'][0:2]!=[5,0]:raise RuntimeError('Explicit seed failed: '+str(result['seed']))
    return result,b''
   if phase=='write':
    result['before_storage']=words(monitor,symbol(mapping,'rf_scene_checkpoint_fallback_before_storage'),8)
    result['after_storage']=words(monitor,symbol(mapping,'rf_scene_checkpoint_fallback_after_storage'),8)
    result['before_slots']=words(monitor,symbol(mapping,'rf_scene_checkpoint_fallback_before_slots'),6)
    result['after_slots']=words(monitor,symbol(mapping,'rf_scene_checkpoint_fallback_after_slots'),6)
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
   suffix=checkpoint_format(data,player_checkpoint)
   if player_checkpoint:
    player=words(monitor,symbol(mapping,'rf_scene_player_checkpoint_state'),8)
    if player[:5]!=[1,1,1,0,1] or player[5]!=len(data)-576 or player[6]!=3 or player[7]!=1:
     raise RuntimeError('RFCP player load/save/settled operation failed: '+str(player))
    result['player_checkpoint']=player
    result['player_record']=dict(health=struct.unpack_from('<f',data,56)[0],armor=struct.unpack_from('<f',data,60)[0],position=list(struct.unpack_from('<3f',data,64)),weapon=struct.unpack_from('<I',data,52)[0])
   (run/(phase+suffix)).write_bytes(data);result.update(checkpoint=check,checkpoint_memory=memory,sha256=hashlib.sha256(data).hexdigest())
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
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--run',action='store_true');p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--input',type=Path,required=True,help='Idle replay shared by all three phases; no terrain edits')
 p.add_argument('--player-checkpoint',action='store_true',help='Require composed RFCP player plus destruction fallback and telemetry');p.add_argument('--shallow',type=int,choices=[0,1,2,3],default=0);p.add_argument('--seconds',type=int,default=300);a=p.parse_args()
 if not 30<=a.seconds<=3600:p.error('Require30..3600seconds')
 payload,frames=replay(a.input);fixtures=a.fixtures.resolve();suffix='.rfcp' if a.player_checkpoint else '.rfds';expected=(fixtures/('baseline'+suffix)).read_bytes();bad=(fixtures/('candidate'+suffix)).read_bytes()
 checkpoint_format(expected,a.player_checkpoint);checkpoint_format(bad,a.player_checkpoint)
 original=[(fixtures/f'seed-slot{i}.rfsg').read_bytes() for i in range(2)]
 if original!=[envelope(expected,1),envelope(bad,2)] or expected==bad:raise ValueError('Fixture envelopes invalid')
 mapping=(ROOT/'build/xbox/main.map').read_text()
 for name in ('rf_diagnostic','rf_xbox_checkpoint_storage_state','rf_scene_geomod_checkpoint_state','rf_scene_geomod_checkpoint_data','rf_scene_geomod_checkpoint_memory','rf_xbox_checkpoint_fixture_seed_state','rf_scene_checkpoint_fallback_before_storage','rf_scene_checkpoint_fallback_after_storage','rf_scene_checkpoint_fallback_before_slots','rf_scene_checkpoint_fallback_after_slots'):symbol(mapping,name)
 if a.player_checkpoint:symbol(mapping,'rf_scene_player_checkpoint_state')
 base=ROOT/'local/xemu-harness/pacing-base.qcow2';emulator=Path('C:/Games/Emulators/Xemu');packer=Path('C:/nxdk/tools/extract-xiso/build/extract-xiso.exe')
 for path in (base,packer,emulator/'xemu.exe',emulator/'eeprom.bin',ROOT/'build/xbox/disc/default.xbe'):
  if not path.is_file():raise FileNotFoundError(path)
 if not a.run:print('Preflight ready; no staging, copy or launch.');return
 require_no_project_xemu(ROOT)
 # Reuse the isolation helper's strict artifacts/geomod-hdd/<unique>/ destination.
 run=ROOT/'artifacts/geomod-hdd'/('fallback-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'));run.mkdir(parents=True)
 report=dict(result='FAIL',player_checkpoint=a.player_checkpoint,scope='Native pure-validator fallback, protected-envelope hash and exact DEV player/destruction restart; no physical power-loss claim',base=str(base),base_sha256=digest(base),expected_sha256=hashlib.sha256(expected).hexdigest(),xbe_sha256=digest(ROOT/'build/xbox/disc/default.xbe'))
 def need(ok,why):
  if not ok:raise RuntimeError(why)
 try:
  hdd=run/'save-test.qcow2';owned_copy(base,hdd);shutil.copyfile(emulator/'eeprom.bin',run/'eeprom.bin');(run/'main.map').write_text(mapping)
  for phase in ('seed','write','read'):
   require_no_project_xemu(ROOT);need(digest(ROOT/'build/xbox/disc/default.xbe')==report['xbe_sha256'],'XBE changed')
   iso=stage(run,phase,payload,fixtures,a.shallow,packer,a.player_checkpoint);result,data=launch(run,phase,iso,hdd,mapping,frames,a.seconds,emulator,a.player_checkpoint);report[phase]=result
   need(not result.get('forced_termination'),'Owned process required forced termination')
   if phase=='seed':need(result['seed']==[5,0,0,0,fnv(original[0]),fnv(original[1]),len(original[0]),len(original[1])],'Seed readback mismatch')
   else:need(data==expected,phase+' checkpoint differs')
   if phase=='write':
    need(result['before_storage'][0:2]==[3,0] and result['before_storage'][4:7]==[1,0,len(expected)],'Did not select older valid slot')
    need(result['before_slots']==[0,0,fnv(original[0]),len(original[0]),fnv(original[1]),len(original[1])],'Seeded slots changed before save')
    need(result['after_storage'][0:2]==[5,0] and result['after_storage'][4:7]==[2,1,len(expected)] and result['after_storage'][7]&4,'Replacement save not flushed')
    need(result['after_slots']==[0,0,fnv(original[0]),len(original[0]),fnv(envelope(expected,2)),len(expected)+24],'Protected slot changed or replacement wrong')
   if phase=='read':need(result['storage'][4:7]==[2,1,len(expected)],'Fresh process selected wrong generation')
  report['result']='PASS'
 finally:
  report['base_unchanged']=digest(base)==report['base_sha256']
  if not report['base_unchanged']:report['result']='FAIL'
  (run/'report.json').write_text(json.dumps(report,indent=2));print(run,report['result'],flush=True)
 if report['result']!='PASS':raise RuntimeError('HDD base changed')
if __name__=='__main__':main()
