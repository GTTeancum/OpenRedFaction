"""Isolated stock-64-MiB XEMU particle pixel tests; no host input or desktop capture."""
import datetime,hashlib,json,os,re,shutil,socket,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
root=Path(__file__).resolve().parents[1];emulator=Path('C:/Games/Emulators/Xemu')
run=root/'artifacts/xemu'/('particle-pixels-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));run.mkdir(parents=True)
flag=root/'build/xbox/disc/particle-render-test.flag';saved=flag.read_bytes() if flag.exists() else None
process=monitor=None;report={'result':'FAIL','scope':'Native particle shader/blend/fog/depth probes and retained 16-frame texture ownership, age-selected frames 0/7/15 after archive close, physical page recovery, six stretched-geometry GPU fixtures and authored blood-pool texture through composed growth/draw with physical page recovery; no campaign/PS2 parity claim.'}
def build():subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','tools/build-xbox.sh','--repack'],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True,stdout=subprocess.DEVNULL)
try:
 flag.write_bytes(b'1');build();mapping=(root/'build/xbox/main.map').read_text()
 address=int(re.search(r'_rf_particle_pixel_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
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
   try:state=words(monitor,address,20)
   except RuntimeError as exc:
    if 'received 0' not in str(exc):raise
    time.sleep(.5);continue
   report['state']=state
   if state[0]==0x52504658:
    if state[1]&0x80000000:raise RuntimeError(f'Guest failure {state}')
    if state[1]==2:break
   time.sleep(.5)
  else:raise TimeoutError(f'Particle test timeout: {report.get("state")}')
  assert state[2]==12
  expected=[(144,32,48),(160,64,96),(16,160,48),(88,48,72),(255,0,0),(127,0,0),(127,0,0),(63,128,0),(80,96,48),(24,48,80),(32,64,96),(255,0,0)]
  actual=[((p>>16)&255,(p>>8)&255,p&255) for p in state[3:15]]
  pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe')],text=True).splitlines()]
  assert len(pc)==12 and all(abs(a-b)<=2 for rgb,ref in zip(pc,actual) for a,b in zip(rgb,ref)),pc
  report.update(expected_rgb=expected,actual_rgb=actual,pc_rgb=pc)
  assert all(abs(a-b)<=2 for rgb,ref in zip(actual,expected) for a,b in zip(rgb,ref)),actual
  texture_address=int(re.search(r'_rf_particle_texture_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  texture_state=words(monitor,texture_address,1544)
  texture_pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--textures',str(root/'Installed_Game/maps2.vpp')],text=True).splitlines()]
  texture_actual=[((p>>16)&255,(p>>8)&255,p&255) for p in texture_state[8:]]
  differences=[abs(a-b) for pixel,ref in zip(texture_actual,texture_pc) for a,b in zip(pixel,ref)]
  report['textures']=dict(state=texture_state[:8],actual_rgb=texture_actual,pc_rgb=texture_pc,max_channel_error=max(differences),differing_channels=sum(d!=0 for d in differences))
  assert texture_state[1:3]==[2,6] and len(texture_pc)==1536
  assert texture_state[4:6]==[16,262484], 'All 16 decoded frames must remain resident'
  assert max(differences)<=2,report['textures']['max_channel_error']
  assert texture_state[3]>0 and texture_state[6]<texture_state[3] and texture_state[7]>=texture_state[3],texture_state[:8]
  assert texture_actual[:256]!=texture_actual[256:512], 'Distinct animation frames must differ'
  assert texture_actual[512:768]!=texture_actual[1280:1536], 'Ordinary and additive blending must differ'
  stretch_address=int(re.search(r'_rf_particle_stretch_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  stretch=words(monitor,stretch_address,1544)
  lines=subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--stretch'],text=True).splitlines()
  counts=[int(lines[i*257]) for i in range(6)]
  reference=[tuple(map(int,lines[i*257+1+j].split())) for i in range(6) for j in range(256)]
  actual=[((p>>16)&255,(p>>8)&255,p&255) for p in stretch[8:]]
  errors=[abs(a-b) for rgb,ref in zip(actual,reference) for a,b in zip(rgb,ref)]
  report['stretch']=dict(state=stretch[:8],pc_counts=counts,max_channel_error=max(errors),actual_rgb=actual,pc_rgb=reference)
  assert stretch[1]==2 and stretch[2:8]==counts and len(actual)==len(reference)==1536
  assert counts[3]==0 and all(counts[i]>=3 for i in (0,1,2,4,5)),counts
  assert max(errors)<=2,report['stretch']['max_channel_error']
  assert all(rgb==(32,64,96) for rgb in actual[768:1024])
  assert all(any(rgb!=(32,64,96) for rgb in actual[i*256:(i+1)*256]) for i in (0,1,2,4,5))
  flash_address=int(re.search(r'\s_rf_flash_pixel_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  flash=words(monitor,flash_address,22)
  flash_pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--flash'],text=True).splitlines()]
  flash_rgb=[((p>>16)&255,(p>>8)&255,p&255) for p in flash[2:]]
  assert flash[:2]==[0x5246464c,2] and len(flash_pc)==20
  errors=[abs(a-b) for rgb,ref in zip(flash_rgb,flash_pc) for a,b in zip(rgb,ref)]
  report['flash']=dict(actual_rgb=flash_rgb,pc_rgb=flash_pc,max_channel_error=max(errors))
  assert max(errors)<=2,report['flash']
  corpse_address=int(re.search(r'\s_rf_corpse_pixel_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  corpse=words(monitor,corpse_address,1032)
  corpse_pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--corpse-pixels',str(root/'Installed_Game/maps_en.vpp')],text=True).splitlines()]
  corpse_rgb=[((p>>16)&255,(p>>8)&255,p&255) for p in corpse[8:]]
  assert corpse[:3]==[0x52464350,2,4] and corpse[4]==16420 and len(corpse_pc)==1024,corpse[:8]
  errors=[abs(a-b) for rgb,ref in zip(corpse_rgb,corpse_pc) for a,b in zip(rgb,ref)]
  report['corpse']=dict(state=corpse[:8],max_channel_error=max(errors),actual_rgb=corpse_rgb,pc_rgb=corpse_pc)
  assert max(errors)<=2,report['corpse']['max_channel_error']
  assert corpse[3]>0 and corpse[5]<corpse[3] and corpse[6]>=corpse[3],corpse[:8]
  assert all(rgb==(32,64,96) for rgb in corpse_rgb[:256])
  assert corpse_rgb[256:512]!=corpse_rgb[512:768] and corpse_rgb[512:768]==corpse_rgb[768:1024]
  packed_address=int(re.search(r'\s_rf_packed_lightmap_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  packed=words(monitor,packed_address,66)
  packed_pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--packed-lightmap'],text=True).splitlines()]
  packed_rgb=[((p>>16)&255,(p>>8)&255,p&255) for p in packed[2:]]
  assert packed[:2]==[0x52464c35,2] and len(packed_pc)==64
  errors=[abs(a-b) for rgb,ref in zip(packed_rgb,packed_pc) for a,b in zip(rgb,ref)]
  report['packed_lightmap']=dict(max_channel_error=max(errors),actual_rgb=packed_rgb,pc_rgb=packed_pc)
  assert max(errors)<=2,report['packed_lightmap']
  assert all(rgb==(32,64,96) for rgb in packed_rgb[32:])
  assert packed_rgb[0]!=packed_rgb[31]
  sample_address=int(re.search(r'\s_rf_packed_lightmap_samples\s+([0-9a-fA-F]+)',mapping)[1],16)
  samples=words(monitor,sample_address,132)
  pc_samples=[int(v) for v in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--packed-lightmap-samples'],text=True).split()]
  expected=[]
  for i in range(65):
   v=i%32 if i<64 else 0
   expected.extend([0,0xff000000|(v*8)|((31-v)*8<<8)|((v^15)*8<<16)])
  expected.extend([0xfffffffc,0x12345678])
  assert samples==pc_samples==expected
  report['packed_cpu_samples']=dict(cases=66,result='PASS',scope='Shared renderer-owned image, all64 texels plus u1 row crossing and final overread guard; exact CPU channel*8 values, ignoring alpha bit, PC linear vs Xbox swizzled.')
  corona_address=int(re.search(r'\s_rf_corona_pixel_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  corona=words(monitor,corona_address,38)
  corona_pc=[tuple(map(int,line.split())) for line in subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--corona'],text=True).splitlines()]
  corona_rgb=[((p>>16)&255,(p>>8)&255,p&255) for p in corona[2:]]
  assert corona[:2]==[0x52464352,2] and len(corona_pc)==36
  errors=[abs(a-b) for rgb,ref in zip(corona_rgb,corona_pc) for a,b in zip(rgb,ref)]
  report['corona']=dict(actual_rgb=corona_rgb,pc_rgb=corona_pc,max_channel_error=max(errors))
  assert max(errors)<=2,report['corona']
  animation_address=int(re.search(r'\s_rf_corona_animation_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  animation=words(monitor,animation_address,2064)
  lines=subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--corona-animation',str(root/'Installed_Game/maps4.vpp')],text=True).splitlines()
  frames=[int(lines[i*257]) for i in range(8)]
  pc=[tuple(map(int,lines[i*257+1+j].split())) for i in range(8) for j in range(256)]
  actual=[((p>>16)&255,(p>>8)&255,p&255) for p in animation[16:]]
  assert animation[:5]==[0x52464341,2,5,15,82040] and animation[8:16]==frames==[0,0,1,2,3,4,0,0],animation[:16]
  assert animation[5]>animation[6] and animation[7]>=animation[5],animation[:8]
  errors=[abs(a-b) for rgb,ref in zip(actual,pc) for a,b in zip(rgb,ref)]
  assert len(actual)==len(pc)==2048 and max(errors)<=2,max(errors)
  assert actual[:256]==actual[256:512]==actual[1536:1792]==actual[1792:]
  assert len({tuple(actual[i*256:(i+1)*256]) for i in (0,2,3,4,5)})==5
  report['corona_animation']=dict(state=animation[:16],max_channel_error=max(errors),pixels=len(actual),actual_rgb=actual,pc_rgb=pc)
  animation_address=int(re.search(r'\s_rf_volume_animation_diagnostic\s+([0-9a-fA-F]+)',mapping)[1],16)
  animation=words(monitor,animation_address,2064)
  lines=subprocess.check_output([str(root/'build/pc/Release/rf_particle_pixel_probe.exe'),'--volume-animation',str(root/'Installed_Game/maps3.vpp')],text=True).splitlines()
  frames=[int(lines[i*257]) for i in range(8)]
  pc=[tuple(map(int,lines[i*257+1+j].split())) for i in range(8) for j in range(256)]
  actual=[((p>>16)&255,(p>>8)&255,p&255) for p in animation[16:]]
  assert animation[:5]==[0x52465641,2,21,15,344504] and animation[8:16]==frames==[0,0,1,2,3,4,5,10],animation[:16]
  assert animation[5]>animation[6] and animation[7]>=animation[5],animation[:8]
  errors=[abs(a-b) for rgb,ref in zip(actual,pc) for a,b in zip(rgb,ref)]
  assert len(actual)==len(pc)==2048 and max(errors)<=2,max(errors)
  assert actual[:256]==actual[256:512]
  assert len({tuple(actual[i*256:(i+1)*256]) for i in (0,2,3,4,5,6,7)})==7
  report['volume_animation']=dict(state=animation[:16],max_channel_error=max(errors),pixels=len(actual),actual_rgb=actual,pc_rgb=pc)
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
 build();(run/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(run/'report.json',{'result':report['result'],'state':report.get('state'),'textures':{k:v for k,v in report.get('textures',{}).items() if k not in ('actual_rgb','pc_rgb')}},flush=True)
