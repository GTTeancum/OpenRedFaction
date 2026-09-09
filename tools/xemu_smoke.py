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
    args = parser.parse_args()
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
    symbol = re.search(r'\s[0-9a-fA-F]+:[0-9a-fA-F]+\s+_rf_diagnostic\s+([0-9a-fA-F]+)', map_text)
    if not symbol:
        raise RuntimeError('Diagnostic symbol absent from matching linker map')
    address = int(symbol[1], 16)
    run = root / 'artifacts/xemu' / datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    run.mkdir(parents=True)
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
                if len(words) == 58 and words[:3] == [0x52464447, 9, 5]:
                    replacements=[];skin_checksum=0
                    if args.skin:
                        assets=subprocess.check_output([str(root/'build/pc/Release/rf_entity_assets_probe.exe'),str(root/'Installed_Game/tables.vpp'),'miner1',args.skin],text=True).splitlines()
                        replacements=assets[1:]
                        skin_checksum=int(subprocess.check_output([str(root/'build/pc/Release/rf_checksum_driver.exe')],input=args.skin.encode('ascii').hex()+'\n',text=True).strip(),16)
                    if args.scene and (args.skin or words[31]!=(5 if args.scene_states else 4 if args.scene_stream else 3) or words[56:58]!=[9858,7215] or words[36]!=(8598 if args.scene_states else 8607 if args.scene_stream else 8562)):
                        raise RuntimeError('Combined scene camera/UID/draw ranges differ from fixture')
                    if not args.scene and words[56:58]!=[skin_checksum,len(replacements)]:
                        raise RuntimeError('Guest skin selection differs from requested reference')
                    report['skin']=dict(name=args.skin,checksum=skin_checksum,replacements=len(replacements))
                    if args.scene:report['actor']=dict(uid=words[56],world_vertices=words[57],actor_vertices=words[36]-words[57],camera='Diagnostic 2.2 units in front of authored actor; not gameplay eye')
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
                    vertex_capacity=1024*1024+words[57]*56 if words[31] in (4,5) else 1024*1024 if words[31]==2 else words[36]*56
                    if not 0 < words[44] <= words[47] <= words[3] or words[45:47] != [expected_gpu_bytes, vertex_capacity]:
                        raise RuntimeError('Unexpected GPU allocation or memory telemetry')
                    report['renderer_memory'] = dict(available_bytes_after_upload=words[44]*4096,
                        gpu_image_requested_bytes=words[45], gpu_vertex_requested_bytes=words[46],
                        available_bytes_after_cpu_mesh_release=words[47]*4096,
                        scope='Observed retained diagnostic frame, not full-game peak')
                    report['materials'] = dict(loaded=words[38], allocated_bytes=words[39], pixel_checksum=hex(words[40]), missing=words[41], available_pages=words[42],scope='Validated resident level materials; model GPU image bytes are checked separately')
                    if words[33:35] != [640, 480] or words[35] < 640*4 or words[36] == 0 or words[37] != (64 if words[31] in (2,4,5) else 3):
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
