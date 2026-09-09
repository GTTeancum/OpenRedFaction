"""Launch only this diagnostic in isolated XEMU; no host input or desktop capture.

Uses the UT99 harness approach of linker-map-resolved guest RAM telemetry,
implemented here via QMP. HDD writes use a temporary snapshot; EEPROM is copied.
"""
import argparse
import datetime
import hashlib
import io
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path
from xemu_guest_snapshot import snapshot as guest_snapshot


class Monitor:
    def __init__(self, port):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=2)
        # Use a bounded response wait distinct from the connection timeout.
        self.sock.settimeout(30)
        self.stream = self.sock.makefile('rwb')
        self.receive()
        self.command('qmp_capabilities')

    def receive(self):
        line = self.stream.readline()
        if not line:
            raise RuntimeError('QMP disconnected')
        return json.loads(line)

    def command(self, command, arguments=None):
        request = dict(execute=command)
        if arguments is not None:
            request['arguments'] = arguments
        self.stream.write(json.dumps(request).encode() + b'\n')
        self.stream.flush()
        while True:
            response = self.receive()
            if 'error' in response:
                raise RuntimeError(response['error'])
            if 'return' in response:
                return response['return']

    def close(self):
        self.stream.close()
        self.sock.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--xemu-root', type=Path, default=Path('C:/Games/Emulators/Xemu'))
    parser.add_argument('--port', type=int, default=46271)
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--bios', default='xbox-4627_debug.bin')
    parser.add_argument('--display', choices=['none', 'xemu'], default='xemu')
    parser.add_argument('--reference', type=Path, help='Require framebuffer comparison against this PC reference image')
    parser.add_argument('--no-capture', action='store_true', help='Validate runtime telemetry without capturing a framebuffer')
    parser.add_argument('--skin',help='Expected miner1 skin selected by the guest model-skin.txt file')
    parser.add_argument('--scene',action='store_true',help='Expect the close Live Mines / miner 9858 combined fixture')
    parser.add_argument('--scene-stream',action='store_true',help='Expect 64 combined scene frames, retained frame 63')
    parser.add_argument('--scene-states',action='store_true',help='Expect authored-state scene playback')
    parser.add_argument('--actor-body',action='store_true',help='Expect actor-body.flag per-frame passive physics')
    parser.add_argument('--actor-drive',action='store_true',help='Expect actor-drive.flag process-local steering pulse')
    parser.add_argument('--door-view',action='store_true',help='Expect door-view.flag camera on mover 8544 during authored-state playback')
    parser.add_argument('--door-motion',action='store_true',help='Expect door-motion.flag to draw 600 simultaneous door updates at 1/60-second steps')
    parser.add_argument('--door-motion-frames',type=int,default=600,help='Expected optional door-motion-frames.txt diagnostic endpoint (1..600)')
    parser.add_argument('--actor-contact',action='store_true',help='Expect actor-contact.flag sustained -X collision route')
    args = parser.parse_args()
    if args.actor_contact:args.actor_drive=True
    if args.actor_drive:args.actor_body=True
    if args.actor_body:args.scene_states=True
    if args.door_motion:args.door_view=True
    if not 1<=args.door_motion_frames<=600:parser.error('door motion frames must be 1..600')
    if args.door_view:args.scene_states=True
    if args.scene_states:args.scene_stream=True
    if args.scene_stream:args.scene=True
    if args.no_capture and args.reference is not None:
        parser.error('--reference requires framebuffer capture')
    root = Path(__file__).resolve().parents[1]
    build = root / 'build/xbox'
    animation_reference = list(struct.unpack('<8I', subprocess.check_output([
        str(root/'build/pc/Release/rf_animation_check.exe'),
        str(root/'Installed_Game/meshes.vpp'), str(root/'Installed_Game/motions.vpp')])))
    map_text = (build / 'main.map').read_text()
    actor_physics_reference=None
    if args.scene_states:
        actor_physics_symbol=re.search(r'_rf_scene_actor_physics_diagnostic\s+([0-9a-fA-F]+)',map_text)
        if not actor_physics_symbol:raise RuntimeError('Integrated actor physics symbol absent')
        scene_args=[str(root/'build/pc/Release/rf_scene_check.exe'),str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl','9858']
        scene_args += [str(root/'Installed_Game'/n) for n in ['meshes.vpp','motions.vpp','tables.vpp','maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp']]
        output=subprocess.check_output(scene_args+['--contact' if args.actor_contact else '--drive' if args.actor_drive else '--body' if args.actor_body else '--states'],text=True)
        actor_final_vertices=int(re.search(r'Frame 63 actor triangles (\d+)',output)[1])*3
        actor_frame_reference=[(int(n)*3,int(h,16)) for n,h in re.findall(r'Frame \d+ actor triangles (\d+) hash ([0-9a-f]+)',output)][:64]
        if args.actor_body:actor_tick_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_TICKS ')).split()[1:]))
        if args.actor_body:actor_ground_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_GROUND ')).split()[1:]))
        if args.actor_body:actor_landing_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_LANDING ')).split()[1:]))
        if args.actor_body:actor_movement_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_MOVEMENT ')).split()[1:]))
        if args.actor_body:actor_speed_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_SPEED ')).split()[1:]))
        if args.actor_body:actor_ground_modes_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_GROUND_MODES ')).split()[1:]))
        if args.actor_body:actor_contact_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_CONTACTS ')).split()[1:]))
        if args.actor_body:actor_speed_modes_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_SPEED_MODES ')).split()[1:]))
        if args.actor_body:actor_stance_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_STANCE ')).split()[1:]))
        if args.actor_body:actor_stance_cache_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_STANCE_CACHE ')).split()[1:]))
        if args.actor_body:actor_input_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_INPUT ')).split()[1:]))
        actor_physics_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('PHYSICS ')).split()[1:]))
        actor_world_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_WORLD ')).split()[1:]))
        actor_fall_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_FALL ')).split()[1:]))
        actor_time_reference=list(map(int,next(line for line in output.splitlines() if line.startswith('ACTOR_TIME ')).split()[1:]))
    symbol = re.search(r'\s[0-9a-fA-F]+:[0-9a-fA-F]+\s+_rf_diagnostic\s+([0-9a-fA-F]+)', map_text)
    if not symbol:
        raise RuntimeError('Diagnostic symbol absent from matching linker map')
    address = int(symbol[1], 16)
    collision_symbol=re.search(r'_rf_collision_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not collision_symbol:raise RuntimeError('Collision diagnostic symbol absent')
    collision_reference=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl']).split()))
    sweep_symbol=re.search(r'_rf_sweep_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not sweep_symbol:raise RuntimeError('Sweep diagnostic symbol absent')
    sweep_reference=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--world-sweep',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl']).split()))
    mover_symbol=re.search(r'_rf_mover_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not mover_symbol:raise RuntimeError('Mover diagnostic symbol absent')
    mover_reference=list(map(int,subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--registered-combined-world',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl']).split()))
    logic_symbol=re.search(r'_rf_level_logic_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not logic_symbol:raise RuntimeError('Owned level logic symbol absent')
    logic_payload=bytearray();logic_counts=[];logic_links=[];logic_bytes=32
    for kind,size,stride in [('triggers',668,672),('events',1144,1148)]:
        payload=subprocess.check_output([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl','--'+kind])
        count,=struct.unpack_from('<I',payload);logic_counts.append(count);cursor=4;links=0
        for i in range(count):
            link_count,=struct.unpack_from('<I',payload,cursor+16);links+=link_count;cursor+=size+4*link_count
        assert cursor==len(payload)
        logic_links.append(links);logic_bytes+=count*stride+4*links;logic_payload+=payload[4:]
    entity_run=subprocess.run([str(root/'build/pc/Release/rf_level_entity_probe.exe'),str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl','--owned-entities'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
    entity_payload=entity_run.stdout;entity_count=0;entity_bytes=16;cursor=0
    while cursor<len(entity_payload):
        record_bytes,=struct.unpack_from('<I',entity_payload,cursor+1080)
        cursor+=1084+record_bytes;entity_count+=1;entity_bytes+=1088+record_bytes
    assert cursor==len(entity_payload)
    logic_bytes+=entity_bytes;logic_payload+=entity_payload
    logic_hash=2166136261
    for byte in logic_payload:logic_hash=((logic_hash^byte)*16777619)&0xffffffff
    registry_symbol=re.search(r'_rf_registry_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not registry_symbol:raise RuntimeError('Registry diagnostic absent')
    group_symbol=re.search(r'_rf_group_storage_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not group_symbol:raise RuntimeError('Owned group diagnostic symbol absent')
    group_raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--owned-groups',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl'])
    group_count,group_bytes=struct.unpack_from('<2I',group_raw);group_hash=2166136261
    for byte in group_raw[8:]:group_hash=((group_hash^byte)*16777619)&0xffffffff
    group_inventory=next(l for l in json.loads((root/'artifacts/moving-groups.json').read_text())['results'] if l['file'].lower()=='l1s1.rfl')
    group_totals=[sum(len(g[field]) for g in group_inventory['records']) for field in ('keys','ids1','legacy')]
    group_totals[1]+=sum(len(g['ids2']) for g in group_inventory['records'])
    runtime_symbol=re.search(r'_rf_group_runtime_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not runtime_symbol:raise RuntimeError('Controller runtime diagnostic symbol absent')
    runtime_raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--runtime-groups',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl'])
    runtime_header=list(struct.unpack_from('<4I',runtime_raw));runtime_hash=2166136261
    for byte in runtime_raw[16:]:runtime_hash=((runtime_hash^byte)*16777619)&0xffffffff
    membership_symbol=re.search(r'_rf_group_membership_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not membership_symbol:raise RuntimeError('Membership diagnostic symbol absent')
    membership_raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--registered-member-groups',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl'])
    membership_header=list(struct.unpack_from('<4I',membership_raw));membership_hash=2166136261
    for byte in membership_raw[16:]:membership_hash=((membership_hash^byte)*16777619)&0xffffffff
    membership_at=16+membership_header[1]*20;membership_accepted=0
    for _ in range(membership_header[0]):
        n,=struct.unpack_from('<I',membership_raw,membership_at);membership_accepted+=n;membership_at+=8+4*n
    if membership_at!=len(membership_raw):raise RuntimeError('Malformed PC membership reference')
    motion_symbol=re.search(r'_rf_door_motion_diagnostic\s+([0-9a-fA-F]+)',map_text)
    if not motion_symbol:raise RuntimeError('Door motion diagnostic symbol absent')
    bound_raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--bound-movers',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl'])
    mover_poses={struct.unpack_from('<i',bound_raw,12+368*i)[0]:bound_raw[144+368*i:380+368*i] for i in range(struct.unpack_from('<I',bound_raw)[0])}
    final_runtime=bytearray(runtime_raw[16:]);motion_hash=2166136261;motion_doors=0;motion_ticks=0;render_poses=[];door_traces=[]
    steps=args.door_motion_frames if args.door_motion else 40
    for gi,g in enumerate(group_inventory['records']):
        if g['flags'][1]:continue
        at=16+320*gi;uid=g['ids2'][0];keywire=b''.join(struct.pack('<8f',*k['position'],*k['timing']) for k in g['keys'])
        trace=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--door-cycle-smooth' if args.door_motion else '--door-cycle',str(steps)],input=runtime_raw[at+8:at+320]+mover_poses[uid]+keywire)
        if len(trace)!=steps*552:raise RuntimeError('Unexpected PC door trace length')
        door_traces.append((uid,trace))
        last=trace[-552:];final_runtime[320*gi+8:320*gi+320]=last[4:316];motion_doors+=1;motion_ticks+=steps
    ordered=[(uid,trace[frame*552:(frame+1)*552]) for frame in range(steps) for uid,trace in door_traces] if args.door_motion else [(uid,trace[frame*552:(frame+1)*552]) for uid,trace in door_traces for frame in range(steps)]
    for index,(uid,tick) in enumerate(ordered):
        for byte in tick:motion_hash=((motion_hash^byte)*16777619)&0xffffffff
        mover_poses[uid]=tick[316:552]
        if args.door_motion and (index+1)%motion_doors==0:render_poses.append(b''.join(mover_poses.values()))
    final_runtime_hash=2166136261
    for byte in final_runtime:final_runtime_hash=((final_runtime_hash^byte)*16777619)&0xffffffff
    view_hash=2166136261
    for pose in mover_poses.values():
        view=pose[212:236]+pose[68:80]+pose[104:140]+pose[56:68]+pose[140:176]
        for byte in view:view_hash=((view_hash^byte)*16777619)&0xffffffff
    run = root / 'artifacts/xemu' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    run.mkdir(parents=True)
    render_reference=[];render_hash=2166136261
    if args.door_motion:
        render_symbol=re.search(r'_rf_door_render_diagnostic\s+([0-9a-fA-F]+)',map_text)
        if not render_symbol:raise RuntimeError('Door render diagnostic symbol absent')
        pose_path=run/'door-poses.bin';pose_path.write_bytes(b''.join(render_poses))
        (root/'artifacts/door-render-final.bin').write_bytes(render_poses[-1])
        render_reference=[list(map(int,line.split())) for line in subprocess.check_output([
            str(root/'build/pc/Release/rf_scene_check.exe'),'--pose-world',str(root/'Installed_Game/levels1.vpp'),'L1S1.rfl',str(pose_path),str(run/'door-pc-mesh.bin')],text=True).splitlines()]
        if len(render_reference)!=steps:raise RuntimeError('Wrong PC render frame count')
        for pair in render_reference:
            for byte in struct.pack('<2I',*pair):render_hash=((render_hash^byte)*16777619)&0xffffffff
        (run/'door-render-reference.json').write_text(json.dumps(render_reference))
    eeprom = run / 'eeprom.bin'
    shutil.copyfile(args.xemu_root / 'eeprom.bin', eeprom)
    config = run / 'xemu.toml'
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
[sys.files]
bootrom_path = '{(args.xemu_root / 'MCPX/mcpx_1.0.bin').as_posix()}'
flashrom_path = '{(args.xemu_root / 'BIOS' / args.bios).as_posix()}'
eeprom_path = '{eeprom.as_posix()}'
hdd_path = '{(args.xemu_root / 'HDD/xbox_hdd.qcow2').as_posix()}'
dvd_path = '{(build / 'redfaction-diagnostic.iso').as_posix()}'
""", encoding='utf-8')
    command = [str(args.xemu_root / 'xemu.exe'), '-config_path', str(config), '-m', '64',
               '-snapshot', '-display', args.display, '-audio', 'none',
               '-qmp', f'tcp:127.0.0.1:{args.port},server=on,wait=off']
    report = dict(command=command, address=hex(address), result='FAIL',
                  capture_requested=not args.no_capture,
                  animation_reference=animation_reference,
                  xbe_sha256=hashlib.sha256((build / 'disc/default.xbe').read_bytes()).hexdigest(),
                  iso_sha256=hashlib.sha256((build / 'redfaction-diagnostic.iso').read_bytes()).hexdigest(), samples=[])
    startup = None
    flags = 0
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        flags = subprocess.CREATE_NO_WINDOW
    monitor = None
    process = None
    try:
        with (run / 'stdout.log').open('wb') as out, (run / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=run, stdout=out, stderr=err, startupinfo=startup, creationflags=flags)
            deadline = time.monotonic() + args.seconds
            next_snapshot=time.monotonic()+15
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'XEMU exited {process.returncode}; inspect {run}')
                if monitor is None:
                    try:
                        monitor = Monitor(args.port)
                    except OSError:
                        time.sleep(0.5)
                        continue
                    report['memory'] = monitor.command('query-memory-size-summary')
                    if report['memory'].get('base-memory') != 64 * 1024 * 1024:
                        raise RuntimeError('XEMU did not report exactly 64 MiB')
                reply = monitor.command('human-monitor-command', {'command-line': f'x /58wx 0x{address:x}'})
                words = []
                for line in reply.splitlines():
                    if ':' in line:
                        words.extend(int(word, 16) for word in re.findall(r'0x[0-9a-fA-F]{8}\b', line.split(':', 1)[1]))
                if words and (not report['samples'] or words != report['samples'][-1]):
                    report['samples'].append(words)
                    print('Guest telemetry:', [hex(w) for w in words], flush=True)
                if time.monotonic()>=next_snapshot:
                    memory_snapshot=guest_snapshot(monitor,map_text)
                    (run/'guest-memory-latest.json').write_text(json.dumps(memory_snapshot,indent=2))
                    report['guest_memory_latest']='guest-memory-latest.json'
                    print('Guest memory:',memory_snapshot.get('authored'),flush=True)
                    next_snapshot=time.monotonic()+15
                if len(words) == 58 and words[:3] == [0x52464447, 9, 5]:
                    collision_reply=monitor.command('human-monitor-command',{'command-line':f'x /9wx 0x{int(collision_symbol[1],16):x}'})
                    collision=[]
                    for line in collision_reply.splitlines():
                        if ':' in line:collision.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    if len(collision)!=9 or collision[:2]!=[0x52464357,1] or collision[2:8]!=collision_reference[4:10] or not 0<collision[8]<=words[3]:
                        raise RuntimeError(f'Guest collision world differs from PC: {collision}; reference {collision_reference}')
                    report['collision_world']=dict(retained_bytes=collision[2],peak_bytes=collision[3],queries=collision[4],hits=collision[5],errors=collision[6],checksum=hex(collision[7]),available_bytes_after_build=collision[8]*4096,scope='Initial Live Mines world retained alongside renderer/animation; room rays match PC. No gameplay movement or mutable-state validation.')
                    sweep_reply=monitor.command('human-monitor-command',{'command-line':f'x /8wx 0x{int(sweep_symbol[1],16):x}'})
                    sweep=[]
                    for line in sweep_reply.splitlines():
                        if ':' in line:sweep.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    want=[sweep_reference[i] for i in (6,7,10,8,9)]
                    if len(sweep)!=8 or sweep[:2]!=[0x52465357,1] or sweep[2:7]!=want or not 0<sweep[7]<=words[3]:
                        raise RuntimeError(f'Guest world sweeps differ from PC: {sweep}; reference {want}')
                    report['collision_sweeps']=dict(queries=sweep[2],hits=sweep[3],edge_hits=sweep[4],errors=sweep[5],checksum=hex(sweep[6]),available_bytes_after_sweeps=sweep[7]*4096,scope='Three finite-radius queries per nonempty Live Mines room; exact PC output hash including level face IDs and edge normals. No actor movement response.')
                    mover_reply=monitor.command('human-monitor-command',{'command-line':f'x /12wx 0x{int(mover_symbol[1],16):x}'})
                    mover=[]
                    for line in mover_reply.splitlines():
                        if ':' in line:mover.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    want=mover_reference[2:7]
                    if len(mover)!=12 or mover[:3]!=[0x52464d56,1,mover_reference[1]] or mover[5:10]!=want or mover[11] or not 0<mover[10]<=words[3] or mover[3]+collision[2]!=mover_reference[7] or mover[4]<mover[3]:
                        raise RuntimeError(f'Guest combined mover query differs from PC: {mover}; reference {mover_reference}')
                    report['collision_movers']=dict(count=mover[2],retained_bytes=mover[3],peak_bytes=mover[4],queries=mover[5],hits=mover[6],moving_hits=mover[7],static_hits=mover[8],checksum=hex(mover[9]),available_bytes_after_build=mover[10]*4096,scope='Initial owned movers retained with world/rendering. Combined ray output and nullable visibility match PC; registry-assigned handles in explicit mover-first creation order; no general-object construction.')
                    logic_reply=monitor.command('human-monitor-command',{'command-line':f'x /12wx 0x{int(logic_symbol[1],16):x}'})
                    logic=[]
                    if actor_physics_reference is not None:
                        reply=monitor.command('human-monitor-command',{'command-line':f'x /8wx 0x{int(actor_physics_symbol[1],16):x}'})
                        actor_physics=[]
                        for line in reply.splitlines():
                            if ':' in line:actor_physics.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                        if actor_physics!=actor_physics_reference:raise RuntimeError(f'Actor physics mismatch: {actor_physics}; PC {actor_physics_reference}')
                        report['actor_physics']=dict(words=actor_physics,scope='Shared authored config and frame-zero model spheres installed into body; retained across 64 rendered diagnostic frames. Provisional identity tensor and scripted spawn pose; no actor motion response or AI.')
                        memory_snapshot=guest_snapshot(monitor,map_text)
                        if args.actor_body:
                            speed_modes=memory_snapshot['symbols']['rf_scene_actor_movement_frames']['words']
                            if speed_modes!=actor_speed_modes_reference:raise RuntimeError('Actor stance movement settings differ from PC')
                            report['actor_speed_modes_match_pc']=64
                            stance=memory_snapshot['symbols']['rf_scene_actor_stance_frames']['words']
                            cache=memory_snapshot['symbols']['rf_scene_actor_stance_cache']['words']
                            if stance!=actor_stance_reference or cache!=actor_stance_cache_reference:raise RuntimeError('Actor stance cache or transition differs from PC')
                            report['actor_stance']=dict(frames_match_pc=64,cache_bytes=200,crouched_frames=sum(bool(stance[i+1]&0x400) for i in range(0,256,4)),blocked_standing_frames=sum(stance[3::4]),scope='Cached stance-center transitions and stationary-world standing clearance; diagnostic initial pose and controller requests.')
                            inputs=memory_snapshot['symbols']['rf_scene_actor_input_frames']['words']
                            drive=memory_snapshot['symbols']['rf_scene_actor_drive_enabled']['words'][0]
                            if drive!=(2 if args.actor_contact else int(args.actor_drive)) or inputs!=actor_input_reference:raise RuntimeError('Actor process-local input differs from PC or requested fixture')
                            report['actor_input']=dict(drive=bool(drive),profile=drive,frames_match_pc=64,scope='Process-local grounded steering; profile 1: +X .25 frames 24..47; profile 2: -X 1 frames 24..62. No host input.')
                            contact_count=memory_snapshot['symbols']['rf_scene_actor_contact_count']['words'][0]
                            contact_words=memory_snapshot['symbols']['rf_scene_actor_contacts']['words'][:contact_count*25]
                            if [contact_count]+contact_words!=actor_contact_reference:raise RuntimeError('Actor contact inputs/results differ from PC')
                            report['actor_contacts_match_pc']=contact_count
                            frames=memory_snapshot['symbols']['rf_scene_actor_render_frames']['words']
                            if [(frames[i],frames[i+1]) for i in range(0,320,5)]!=actor_frame_reference:raise RuntimeError('Actor rendered frame sequence differs from PC')
                            if frames[2:5]==frames[317:320]:raise RuntimeError('Actor body never moved')
                            report['actor_render_frames_match_pc']=64
                            ticks=memory_snapshot['symbols']['rf_scene_actor_tick_stats']['words']
                            if ticks!=actor_tick_reference:raise RuntimeError(f'Actor tick sequence differs: {ticks}; PC {actor_tick_reference}')
                            report['actor_ticks']=dict(frames=ticks[1],passes=ticks[2],contacts=ticks[3],capped_frames=ticks[4],maximum_passes=ticks[5])
                            ground=memory_snapshot['symbols']['rf_scene_actor_ground_stats']['words']
                            modes=memory_snapshot['symbols']['rf_scene_actor_ground_modes']['words']
                            if modes!=actor_ground_modes_reference:raise RuntimeError('Ground-probe movement modes differ from PC')
                            if ground!=actor_ground_reference:raise RuntimeError(f'Actor ground probes differ: {ground}; PC {actor_ground_reference}')
                            ground_records=memory_snapshot['symbols']['rf_scene_actor_ground_records']['words']
                            ground_hash=2166136261
                            for byte in struct.pack('<2112I',*ground_records):ground_hash=((ground_hash^byte)*16777619)&0xffffffff
                            if ground_hash!=ground[5]:raise RuntimeError('Captured ground record hash differs from guest summary')
                            report['actor_ground']=dict(records=ground[1],hits=ground[2],walkable=ground[3],first_walkable_frame=ground[4],hash=hex(ground[5]),modes=modes,scope='Mode-dependent stationary support probes, landing and moved-grounded support maintenance.')
                            landing=memory_snapshot['symbols']['rf_scene_actor_landing']['words']
                            if landing!=actor_landing_reference:raise RuntimeError('Actor landing differs from PC')
                            movement=memory_snapshot['symbols']['rf_scene_actor_movement']['words']
                            if movement!=actor_movement_reference:raise RuntimeError('Authored run/fall descriptors differ from PC')
                            report['actor_movement']=dict(words=movement,scope='Authored run/fall descriptors; body/disabled translation axes in this fixture.')
                            speed=memory_snapshot['symbols']['rf_scene_actor_movement_values']['words']
                            if speed!=actor_speed_reference:raise RuntimeError('Authored class movement values differ from PC')
                            report['actor_movement_values']=list(struct.unpack('<4f',struct.pack('<4I',*speed)))
                            report['actor_landing']=dict(mode=landing[1],frame=landing[2],transitions=landing[3],grounded_ticks=landing[4],support_commits=landing[6],support_losses=landing[7],scope='Static run landing and moved-grounded support maintenance; no damage/sound/AI or moving platforms.')
                            pose=memory_snapshot['symbols']['rf_scene_actor_pose']['words']
                            body=memory_snapshot['symbols']['scene_actor_body']['words']
                            if not pose[0]&0x4000000 or any(pose[i:i+3]!=body[22:25] for i in (14,17,20)) or body[22:25]!=body[25:28] or pose[53:59]!=body[62:68]:raise RuntimeError('Actor public/current/pending pose or bounds diverged')
                            report['actor_pose_commit_matches_body']=True
                            report['actor_physics']['scope']='Live falling, static run landing and grounded motion with optional process-local input; scripted animation and provisional initial pose/inertia. General movement modes, gameplay lifecycle and AI remain open.'
                        actor_world=memory_snapshot['symbols']['rf_actor_world_diagnostic']['words']
                        if actor_world!=actor_world_reference:raise RuntimeError(f'Actor world sweep mismatch: {actor_world}; PC {actor_world_reference}')
                        report['actor_world']=dict(words=actor_world,scope='Actual retained actor spheres swept two units along six world axes against stationary geometry; diagnostic mask 0x460, no movement response.')
                        actor_fall=memory_snapshot['symbols']['rf_actor_fall_diagnostic']['words']
                        if actor_fall!=actor_fall_reference:raise RuntimeError(f'Actor fall mismatch: {actor_fall}; PC {actor_fall_reference}')
                        report['actor_fall']=dict(words=actor_fall,scope='Passive falling fixture completes the first static-contact frame with bounded remaining-time passes; no damage, full actor pose/room commit or rendered movement.')
                        actor_time=memory_snapshot['symbols']['rf_scene_actor_contact_time']['words']
                        if actor_time!=actor_time_reference:raise RuntimeError(f'Actor contact time mismatch: {actor_time}; PC {actor_time_reference}')
                        report['actor_contact_time']=actor_time
                        config_reference=bytes.fromhex(subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),'--physics-config',str(root/'Installed_Game/tables.vpp'),'miner1'],text=True))
                        config_words=memory_snapshot['symbols']['resident_miner_config']['words']
                        if struct.pack('<117I',*config_words)!=config_reference:raise RuntimeError('Full guest actor configuration differs from PC')
                        report['actor_config_bytes_match_pc']=len(config_reference)
                        (run/'guest-memory-complete.json').write_text(json.dumps(memory_snapshot,indent=2))
                        report['guest_memory_complete']='guest-memory-complete.json'
                    for line in logic_reply.splitlines():
                        if ':' in line:logic.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    logic_want=[0x52464c47,1,*logic_counts,logic_bytes,*logic_links,logic_hash,logic_hash]
                    if len(logic)!=12 or logic[:9]!=logic_want or logic[9]<(66 if args.scene_states else 2) or logic[10:]!=[entity_count,entity_bytes]:
                        raise RuntimeError(f'Owned level logic differs: {logic}; expected {logic_want}')
                    registry_reply=monitor.command('human-monitor-command',{'command-line':f'x /6wx 0x{int(registry_symbol[1],16):x}'})
                    registry=[]
                    for line in registry_reply.splitlines():
                        if ':' in line:registry.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    if len(registry)!=6 or registry[:5]!=[0x52465247,1,membership_header[1],group_count,12300] or registry[5]<(65 if args.scene_states else 1):
                        raise RuntimeError(f'Registry lifetime mismatch: {registry}')
                    report['object_registry']=dict(movers=registry[2],controllers=registry[3],bytes=registry[4],checks=registry[5],scope='Registered resident movers then controllers; original handle algorithm, explicit diagnostic creation order. Trigger/event registration pending.')
                    report['level_logic']=dict(triggers=logic[2],events=logic[3],entities=logic[10],entity_bytes=logic[11],retained_bytes=logic[4],trigger_links=logic[5],event_links=logic[6],hash=hex(logic[8]),lifetime_checks=logic[9],scope='Owned trigger/event records and links plus decoded/raw entity records match PC through archive closure; no gameplay entity registration or activation.')
                    group_reply=monitor.command('human-monitor-command',{'command-line':f'x /10wx 0x{int(group_symbol[1],16):x}'})
                    groups=[]
                    for line in group_reply.splitlines():
                        if ':' in line:groups.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    group_want=[0x52464753,1,group_count,group_bytes]+group_totals+[group_hash,group_hash]
                    if len(groups)!=10 or groups[:9]!=group_want or groups[9]<(66 if args.scene_states else 2):
                        raise RuntimeError(f'Guest owned controller data differs from PC or lifetime checks missing: {groups}; expected {group_want}')
                    report['controller_storage']=dict(groups=groups[2],retained_bytes=groups[3],keys=groups[4],ids=groups[5],legacy_poses=groups[6],checksum=hex(groups[8]),lifetime_checks=groups[9],scope='Owned authored controller data matches PC after rendered frames and level archive closure; no controller playback or registration.')
                    runtime_reply=monitor.command('human-monitor-command',{'command-line':f'x /8wx 0x{int(runtime_symbol[1],16):x}'})
                    runtime=[]
                    for line in runtime_reply.splitlines():
                        if ':' in line:runtime.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    if runtime!=[0x52464752,1]+runtime_header+[runtime_hash,final_runtime_hash]:raise RuntimeError(f'Guest controller runtime differs from PC: {runtime}')
                    report['controller_runtime']=dict(groups=runtime[2],retained_bytes=runtime[3],translations=runtime[4],rotation_pending=runtime[5],checksum=hex(runtime[7]),lifetime_checks=groups[9],scope='Initial runtime integrity through rendering/archive closure; final runtime matches PC after controlled door cycles. Rotation remains pending.')
                    membership_reply=monitor.command('human-monitor-command',{'command-line':f'x /11wx 0x{int(membership_symbol[1],16):x}'})
                    memberships=[]
                    for line in membership_reply.splitlines():
                        if ':' in line:memberships.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    want=[0x5246474d,1]+membership_header+[membership_accepted,membership_hash,membership_hash]
                    if len(memberships)!=11 or memberships[:9]!=want or memberships[9]!=groups[9] or memberships[10]!=20*membership_header[1]+4*membership_header[0]:
                        raise RuntimeError(f'Guest mover memberships differ from PC: {memberships}; expected {want}')
                    report['controller_memberships']=dict(groups=memberships[2],objects=memberships[3],retained_bytes=memberships[4],peak_bytes=memberships[5],references=memberships[6],checksum=hex(memberships[8]),lifetime_checks=memberships[9],object_and_controller_table_bytes=memberships[10],scope='Authored mover lists and object parents/flags match PC through rendering/archive closure; pose flags and collision IDs synchronized. Registry-assigned handles; no general-object binding or authored activation.')
                    motion_reply=monitor.command('human-monitor-command',{'command-line':f'x /8wx 0x{int(motion_symbol[1],16):x}'})
                    motion=[]
                    for line in motion_reply.splitlines():
                        if ':' in line:motion.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                    if len(motion)!=8 or motion[:6]!=[0x5246444d,1,motion_doors,motion_ticks,motion_hash,view_hash] or not 0<motion[6]<=words[3] or motion[7]!=membership_header[1]*8:
                        raise RuntimeError(f'Guest connected door motion differs from PC: {motion}')
                    report['door_motion']=dict(doors=motion[2],ticks=motion[3],trace_checksum=hex(motion[4]),collision_view_checksum=hex(motion[5]),available_bytes_during_motion=motion[6]*4096,scratch_bytes=motion[7],scope='Resident controller/mover activation, tick, propagation and commit match original-verified PC traces after archive closure; collision views synchronized each tick. Unobstructed diagnostic, no sound dispatch, crate rotation or mover rendering.')
                    replacements=[];skin_checksum=0
                    if args.skin:
                        assets=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),str(root/'Installed_Game/tables.vpp'),'miner1',args.skin],text=True).splitlines()
                        replacements=assets[1:]
                        skin_checksum=int(subprocess.check_output([str(root/'build/pc/Release/rf_checksum_driver.exe')],input=args.skin.encode('ascii').hex()+'\n',text=True).strip(),16)
                    if args.door_motion:
                        render_reply=monitor.command('human-monitor-command', {'command-line':f'x /10wx 0x{int(render_symbol.group(1),16):x}'})
                        render=[]
                        for line in render_reply.splitlines():
                            if ':' in line:render.extend(int(w,16) for w in re.findall(r'0x[0-9a-fA-F]{8}\b',line.split(':',1)[1]))
                        if len(render)==10:
                            target=run/'door-xbox-mesh.bin'
                            monitor.command('human-monitor-command', {'command-line':f'memsave 0x{render[8]:x} {render[9]} "{target.as_posix()}"'})
                        if len(render)!=10 or render[:3]!=[0x52464452,1,steps] or render[4]!=1024*1024+2892*56 or render[5:8]!=[render_hash,*render_reference[-1]] or not 0<render[3]<1024*1024:
                            raise RuntimeError(f'Guest rendered door meshes differ from PC: {render}')
                        report['door_render']=dict(frames=render[2],retained_geometry_bytes=render[3],vertex_capacity_bytes=render[4],trace_hash=hex(render[5]),last_vertices=render[6],last_mesh_hash=hex(render[7]),step_seconds=1/60,scope='Every committed pose mesh matches PC after archive closure; one CPU/GPU vertex allocation, changing draw counts. All four panels activated together and stepped before each render; diagnostic scheduling, no gameplay trigger dispatch.')
                    expected_world=render_reference[-1][0] if args.door_motion else 2892 if args.door_view else 7455
                    expected_total=expected_world if args.door_view else 8838 if args.scene_states else 8847 if args.scene_stream else 8802
                    if args.actor_body and not args.door_view:expected_total=expected_world+actor_final_vertices
                    if args.scene and (args.skin or words[31]!=(5 if args.scene_states else 4 if args.scene_stream else 3) or words[56:58]!=[9858,expected_world] or words[36]!=expected_total):
                        raise RuntimeError('Combined scene camera/UID/draw ranges differ from fixture')
                    if not args.scene and words[56:58]!=[skin_checksum,len(replacements)]:
                        raise RuntimeError('Guest skin selection differs from requested reference')
                    report['skin']=dict(name=args.skin,checksum=skin_checksum,replacements=len(replacements))
                    if args.scene:report['actor']=dict(uid=words[56],world_vertices=words[57],actor_vertices=words[36]-words[57],camera='Diagnostic 2.2 units in front of authored actor; not gameplay eye')
                    if args.door_view:report['actor']['camera']='Diagnostic mover 8544 bounds center, six units along thinnest local axis; actor outside view'
                    report['animation'] = dict(actual=words[48:56],expected=animation_reference,
                        scope='64 scripted controller/candidate-helper frames with real sidesteps; hashes of bones, playback/controller/references/candidate effects, eye and cache state; absent rolls and sounds')
                    if words[48:56] != animation_reference:
                        raise RuntimeError('Xbox animation differs from the shared PC check')
                    if words[3] != 16384 or words[5:8] != [29, 1294336, 0x32d7cb85]:
                        raise RuntimeError('Unexpected guest memory or archive results')
                    if words[8:13] != [180, 29, 3572594, 1637649, 1130684]:
                        raise RuntimeError('Unexpected level directory results')
                    inventory = json.loads((root / 'artifacts/inventory.json').read_text())
                    archive = next(f for f in inventory['files'] if f['path'] == 'levels1.vpp')
                    entry = next(e for e in archive['vpp']['entries'] if e['name'] == 'L1S1.rfl')
                    with (Path(inventory['root']) / 'levels1.vpp').open('rb') as original:
                        original.seek(entry['offset'] + 12)
                        player_offset, = struct.unpack('<I', original.read(4))
                        original.seek(entry['offset'] + player_offset + 8)
                        expected_position = list(struct.unpack('<3I', original.read(12)))
                    if words[13:16] != expected_position:
                        raise RuntimeError('Guest spawn position differs from original data')
                    geometry = next(g for g in json.loads((root / 'artifacts/geometry.json').read_text()) if g['file'] == 'L1S1.rfl')
                    expected_counts = [geometry[k] for k in ('textures', 'rooms', 'vertices', 'faces', 'corners', 'mappings')]
                    expected_counts += [geometry['bytes'] + 4 * (geometry['textures'] + geometry['rooms'] + geometry['faces'])]
                    if words[16:23] != expected_counts or words[30] != geometry['tail_bytes']:
                        raise RuntimeError('Guest resident geometry differs from original data')
                    level_record = next(l for l in json.loads((root / 'artifacts/levels.json').read_text()) if l['file'] == 'L1S1.rfl')
                    geometry_section = next(s for s in level_record['sections'] if s['type'] == '0x100')
                    with (Path(inventory['root']) / 'levels1.vpp').open('rb') as original:
                        vertex_start = entry['offset'] + geometry_section['offset'] + 8 + geometry['vertices_offset']
                        original.seek(vertex_start)
                        first = list(struct.unpack('<3I', original.read(12)))
                        original.seek(vertex_start + 12 * (geometry['vertices'] - 1))
                        last = list(struct.unpack('<3I', original.read(12)))
                    if words[24:30] != first + last:
                        raise RuntimeError('Guest decoded vertices differ from disk')
                    from PIL import Image
                    material_report = json.loads((root / 'artifacts/image-tests/report.json').read_text())
                    checksum = 0
                    for material in material_report['materials']:
                        asset_archive = next(a for a in inventory['files'] if a['path'] == material['archive'])
                        asset = next(e for e in asset_archive['vpp']['entries'] if e['name'] == material['name'])
                        with (Path(inventory['root']) / material['archive']).open('rb') as original:
                            original.seek(asset['offset']); data = original.read(asset['size'])
                        pixels = Image.open(io.BytesIO(data)).convert('RGBA')
                        if data[17] & 15 == 0:
                            pixels.putalpha(255)
                        value = 2166136261
                        for byte in pixels.tobytes():
                            value = ((value ^ byte) * 16777619) & 0xffffffff
                        checksum ^= value
                    # Win32/Xbox rf_material: four image words + pointer,
                    # status and archive index = 28 bytes including source format.
                    if words[38:42] != [16, material_report['material_rgba_bytes'] + 17*28, checksum, 1]:
                        raise RuntimeError('Guest material allocations or decoded pixel checksum differ from reference')
                    if words[43] != 23:
                        raise RuntimeError('Guest lightmap load did not report 23 images')
                    report['lightmaps'] = dict(count=words[43], mapping_validation='all resident level mapping indices in range; used by world/combined modes, unused by model-only modes')
                    expected_gpu_bytes=3624964
                    if words[31] in (1,2,3,4,5):
                        names=set()
                        rows=subprocess.check_output([str(root/'build/pc/Release/rf_model_file_probe.exe'),str(root/'Installed_Game/meshes.vpp'),'miner.v3c','--materials'],text=True)
                        material_index=0
                        for row in rows.splitlines():
                            if row.startswith('M '):
                                raw=bytes.fromhex(row.split()[3])
                                for offset in (0,48):
                                    name=raw[offset:offset+32].split(b'\0')[0].decode('ascii').lower()
                                    if replacements and offset==0:name=replacements[material_index].lower()
                                    if name:names.add(name)
                                material_index+=1
                        if replacements and material_index!=len(replacements):raise RuntimeError('Skin/model material count differs')
                        expected_gpu_bytes=3624964 if words[31] in (3,4,5) else 4
                        for name in names:
                            for archive_name in ('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp'):
                                source=next(f for f in inventory['files'] if f['path']==archive_name)
                                asset=next((e for e in source['vpp']['entries'] if e['name'].lower()==name),None)
                                if asset:
                                    with (Path(inventory['root'])/archive_name).open('rb') as original:
                                        original.seek(asset['offset']);data=original.read(asset['size'])
                                    size=Image.open(io.BytesIO(data)).size;expected_gpu_bytes+=size[0]*size[1]*4;break
                            else:raise RuntimeError('Missing model reference texture '+name)
                    elif words[31]!=0:raise RuntimeError('Unknown preview scene')
                    report['scene']='authored-state Live Mines / miner 9858' if words[31]==5 else 'streamed Live Mines / miner 9858' if words[31]==4 else 'Live Mines / miner 9858 close inspection' if words[31]==3 else 'streamed miner inspection' if words[31]==2 else 'posed miner inspection' if words[31] else 'Live Mines static geometry'
                    if args.door_view:report['scene']='Live Mines mover 8544 door inspection; actor-state playback outside view'
                    vertex_capacity=1024*1024+words[57]*56 if words[31] in (4,5) else 1024*1024 if words[31]==2 else words[36]*56
                    if args.door_motion:vertex_capacity=1024*1024+2892*56
                    if not 0 < words[44] <= words[47] <= words[3] or words[45:47] != [expected_gpu_bytes, vertex_capacity]:
                        raise RuntimeError('Unexpected GPU allocation or memory telemetry')
                    report['renderer_memory'] = dict(available_bytes_after_upload=words[44]*4096,
                        gpu_image_requested_bytes=words[45], gpu_vertex_requested_bytes=words[46],
                        available_bytes_after_cpu_mesh_release=words[47]*4096,
                        scope='Observed retained diagnostic frame, not full-game peak')
                    if args.door_motion:
                        report['renderer_memory']['available_bytes_with_retained_cpu_mesh']=report['renderer_memory'].pop('available_bytes_after_cpu_mesh_release')
                        report['door_motion']['scope']='Resident simultaneous door controller/mover ticks and collision views match PC; 1/60-second diagnostic steps, unobstructed gates, absent sound/event dispatch and crate rotation.'
                    report['materials'] = dict(loaded=words[38], allocated_bytes=words[39], pixel_checksum=hex(words[40]), missing=words[41], available_pages=words[42],scope='Validated resident level materials; model GPU image bytes are checked separately')
                    if words[33:35] != [640, 480] or words[35] < 640*4 or words[36] == 0 or words[37] != (64+steps if args.door_motion else 64 if words[31] in (2,4,5) else 3):
                        raise RuntimeError('Invalid native renderer capture descriptor')
                    if not args.no_capture:
                        capture = run / 'framebuffer.bin'
                        monitor.command('stop')
                        monitor.command('human-monitor-command', {'command-line': f'pmemsave 0x{words[32] & 0x03ffffff:x} {words[35]*480} "{capture.as_posix()}"'})
                        raw_frame = capture.read_bytes()
                        from PIL import Image
                        frame = Image.frombytes('RGB', (640, 480), raw_frame, 'raw', 'BGRX', words[35], 1)
                        frame.save(run / 'framebuffer.png')
                        report['capture'] = dict(source='Game renderer framebuffer, native guest RAM via QMP', width=640, height=480, triangles=words[36]//3)
                        if args.reference is not None:
                            comparison_path = run / 'comparison.json'
                            comparison = subprocess.run([sys.executable, str(root/'tools/compare_preview.py'),
                                str(args.reference.resolve()), str(run/'framebuffer.png'), '--report', str(comparison_path)], capture_output=True, text=True)
                            if comparison_path.exists():
                                report['comparison'] = json.loads(comparison_path.read_text())
                            if comparison.returncode:
                                raise RuntimeError('PC framebuffer comparison failed: ' + comparison.stdout + comparison.stderr)
                    report['result'] = 'PASS'
                    break
                time.sleep(1)
            if report['result'] != 'PASS':
                report['registers'] = monitor.command('human-monitor-command', {'command-line': 'info registers'}) if monitor else None
                if monitor:
                    for key, diagnostic in [('instructions', 'x /16i $eip'), ('stack', 'x /32wx $esp'), ('kernel_bytes', 'x /64bx 0x80049fc0')]:
                        report[key] = monitor.command('human-monitor-command', {'command-line': diagnostic})
                raise RuntimeError('Diagnostic did not reach archive validation before timeout')
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if monitor is not None:
            try:
                monitor.command('quit')
            except Exception:
                pass
            try:
                monitor.close()
            except OSError:
                pass # Preserve the original failure and still reap/write report.
        if process is not None:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=5)
        (run / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(run, flush=True)
    print('PASS: animation, resident geometry, level and archives on 64 MiB XEMU' +
          ('; framebuffer captured' if not args.no_capture else '; no framebuffer capture'))


if __name__ == '__main__':
    main()
