"""Isolated stock64MiB APU proof using a locally built build/apu-probe image.
No host input, desktop capture, shared disc writes or emulator configuration edits.
"""
import array,datetime,hashlib,json,os,re,shutil,socket,subprocess,time
from pathlib import Path
from xemu_smoke import Monitor
from xemu_guest_snapshot import words
root=Path(__file__).resolve().parents[1];build=root/'build/apu-probe'
emulator=Path('C:/Games/Emulators/Xemu')
run=root/'artifacts/xemu'/('apu-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
run.mkdir(parents=True);mapping=(build/'main.map').read_text()
address=int(re.search(r'_rf_apu_probe\s+([0-9a-fA-F]+)',mapping)[1],16)
shutil.copyfile(emulator/'eeprom.bin',run/'eeprom.bin')
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
eeprom_path = '{run.as_posix()}/eeprom.bin'
hdd_path = '{root.as_posix()}/local/xemu-harness/pacing-base.qcow2'
dvd_path = '{build.as_posix()}/apu-probe.iso'
''')
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
command=[str(emulator/'xemu.exe'),'-config_path',str(config),'-m','64','-snapshot','-display','xemu',
    '-audio',f'wav,id=rf_apu,path={run.as_posix()}/device.wav,out.frequency=48000',
    '-qmp',f'tcp:127.0.0.1:{port},server=on,wait=off']
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
process=None;monitor=None;report=dict(result='FAIL',command=command,samples=[],
    xbe_sha256=hashlib.sha256((build/'disc/default.xbe').read_bytes()).hexdigest(),
    provenance=json.loads((build/'provenance.json').read_text()),
    scope='Isolated original DoorOpen_07 sample through APU voice/DSP on stock64MiB XEMU. Checks natural completion, replay of the retained static buffer, eight stop/destroy/recreate cycles, reinitialization, running-voice left/right/mute DSP routing, synthetic periodic PCM intermediate gain calibration, ten injected initialization allocation failures with restored pages, production adapter sixteen overlapping voices, overflow rejection, slot reuse, stale stop protection, restored available pages and nonzero guest DMA output snapshot. Not a linear audio capture, host audibility, live campaign integration, spatial parity or full backend validation.')
try:
    with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
        environment=dict(os.environ,SDL_AUDIO_DRIVER='dummy')
        process=subprocess.Popen(command,cwd=run,env=environment,stdout=out,stderr=err,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
        deadline=time.monotonic()+60;last=-1
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError(f'XEMU exited {process.returncode}')
            if monitor is None:
                try:monitor=Monitor(port)
                except OSError:time.sleep(.25);continue
                report['memory']=monitor.command('query-memory-size-summary')
                assert report['memory']['base-memory']==64*1024*1024
            try:state=words(monitor,address,14)
            except RuntimeError as exc:
                if 'received 0' not in str(exc):raise
                time.sleep(.25);continue
            if state[0]!=0x52464150:time.sleep(.25);continue
            report['samples'].append(state)
            if state[1]!=last:print('APU stage',state,flush=True);last=state[1]
            assert state[2]==0,state
            if state[1]==9:
                lifecycle_address=int(re.search(r'_rf_apu_lifecycle\s+([0-9a-fA-F]+)',mapping)[1],16)
                lifecycle=words(monitor,lifecycle_address,6);report['lifecycle']=lifecycle
                assert 2400<=lifecycle[0]<=3200 and lifecycle[1:]==[8,8,1,state[3],0],lifecycle
                gain_address=int(re.search(r'_rf_apu_gain_sums\s+([0-9a-fA-F]+)',mapping)[1],16)
                sums=words(monitor,gain_address,10);report['gain_sums']=sums
                assert sums[0]>0 and sums[1]>0,sums
                expected_gains=[1,1,10**(-600/2000),10**(-600/2000),10**(-1000/2000),1,1,10**(-1000/2000),.1,.1]
                ratios=[value/sums[i%2] for i,value in enumerate(sums)];report['gain_ratios']=ratios
                assert all(abs(a-b)<=.03*b for a,b in zip(ratios,expected_gains)),ratios
                channels_address=int(re.search(r'_rf_apu_channel_counts\s+([0-9a-fA-F]+)',mapping)[1],16)
                channels=words(monitor,channels_address,6);report['channel_counts']=channels
                assert channels[0]>0 and channels[3]>0 and all(channels[i]==0 for i in [1,2,4,5]),channels
                failures_address=int(re.search(r'_rf_apu_allocation_failures\s+([0-9a-fA-F]+)',mapping)[1],16)
                report['allocation_failures']=words(monitor,failures_address,1)[0]
                assert report['allocation_failures']==10,report['allocation_failures']
                adapter_address=int(re.search(r'_rf_apu_adapter\s+([0-9a-fA-F]+)',mapping)[1],16)
                adapter=words(monitor,adapter_address,5);report['adapter']=adapter
                assert adapter==[16,1,17,state[3],0],adapter
                assert state[6:9]==[11025,28485,3],state
                assert state[3]==state[5] and state[12]>0,state
                elapsed=(state[11]-state[10])&0xffffffff
                assert 2400<=elapsed<=3200,elapsed
                import struct
                capture_address=int(re.search(r'_rf_apu_dma_snapshot\s+([0-9a-fA-F]+)',mapping)[1],16)
                captured=words(monitor,capture_address,2048)
                (run/'dma-snapshot.bin').write_bytes(struct.pack('<2048I',*captured))
                report['final']=state;report['result']='PASS';break
            time.sleep(.1)
        else:raise RuntimeError('APU probe timed out')
except Exception as exc:
    report['error']=repr(exc)
    if monitor:
        report['registers']=monitor.command('human-monitor-command',{'command-line':'info registers'})
        report['instructions']=monitor.command('human-monitor-command',{'command-line':'x /12i $eip'})
        report['stack']=monitor.command('human-monitor-command',{'command-line':'x /32wx $esp'})
finally:
    if monitor:
        try:monitor.command('quit')
        except Exception:pass
        monitor.close()
    if process:
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
    try:
        # Snapshot of the guest DSP's DMA output ring, not a linear recording
        # and not proof that the host sound device reproduced these samples.
        pcm=(run/'dma-snapshot.bin').read_bytes();assert len(pcm)==8192
        values=array.array('h');values.frombytes(pcm)
        report['capture']=dict(frames=len(pcm)//4,nonzero_samples=sum(v!=0 for v in values),
            peak=max(map(abs,values),default=0),sha256=hashlib.sha256(pcm).hexdigest())
        assert report['capture']['nonzero_samples']>0,'Silent device output'
    except Exception as exc:report['result']='FAIL';report['capture_error']=repr(exc)
    (run/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(run,report['result'],flush=True)
if report['result']!='PASS':raise SystemExit(1)
