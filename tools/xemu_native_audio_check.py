"""Record native Xbox output in an ordinary original L1S1 scene.

No PC build/runtime, host input, screenshots, original-asset editing, generated
level, or direct event dispatch. Optional process-local placement uses the
existing authored-trigger helper; normal contact/timing still runs the event.
Only the parent runs this harness, within the coordinated Xbox validation batch.
"""
import argparse
from array import array
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import re
import select
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import wave

from xemu_guest_snapshot import words
from xemu_host import (PipeMonitor, SessionLock, emulator_binary, emulator_environment,
                       emulator_root, stop_owned_process, xbox_build_command)
from xemu_native_world_save import FLAGS, address
from xemu_session_guard import require_no_project_xemu
from xemu_smoke import Monitor

ROOT = Path(__file__).resolve().parents[1]
DISC = ROOT / 'build/xbox/disc'
COUNTS = {
    'rf_diagnostic': 58, 'rf_player_replay_diagnostic': 4,
    'rf_scene_music': 8, 'rf_xbox_music_diagnostic': 12,
    'rf_xbox_audio_diagnostic': 12, 'rf_scene_message_audio': 4,
    'rf_scene_message_playback': 16, 'rf_scene_trigger_contacts': 6,
    'rf_scene_trigger_history': 4, 'rf_scene_player_life': 8,
    'rf_xbox_level_transitions': 4,
}


class SDLAudioCapture:
    """Paced capture of the real XEMU SDL3 -> ALSA output, Linux only.

    The ALSA plug fixes the file stream at stereo S16_LE/48 kHz. An 8 KiB
    FIFO applies real-time backpressure; a bare null/file PCM would otherwise
    drain as quickly as possible and distort the emulator's audio clock.
    """
    def __init__(self, folder):
        if not sys.platform.startswith('linux'):
            raise RuntimeError('Native SDL/ALSA recording currently requires the Linux cloud harness')
        self.path = folder / 'native-output.wav'
        self.pipe = folder / 'native-output.pcm.pipe'
        self.config = folder / 'capture-alsa.conf'
        self.frames = 0
        self.error = None
        self.stop = threading.Event()
        self.thread = None
        self.fd = None
        self.config.write_text('pcm.rf_native_capture {\n'
            '  type plug\n  slave {\n    pcm {\n      type file\n'
            '      slave.pcm { type null }\n'
            f'      file {json.dumps(str(self.pipe))}\n'
            '      format "raw"\n      truncate true\n    }\n'
            '    format S16_LE\n    channels 2\n    rate 48000\n  }\n}\n')

    def start(self):
        import fcntl
        os.mkfifo(self.pipe, 0o600)
        self.fd = os.open(self.pipe, os.O_RDONLY | os.O_NONBLOCK)
        self.pipe_bytes = fcntl.fcntl(self.fd, fcntl.F_SETPIPE_SZ,
                                      min(8192, fcntl.fcntl(self.fd, fcntl.F_GETPIPE_SZ)))
        self.thread = threading.Thread(target=self._read, name='native-audio-capture', daemon=True)
        self.thread.start()

    def environment(self, folder):
        env = emulator_environment(folder)
        env.update(SDL_AUDIO_DRIVER='alsa', SDL_AUDIODRIVER='alsa',
                   SDL_AUDIO_ALSA_DEFAULT_PLAYBACK_DEVICE='rf_native_capture',
                   SDL_AUDIO_FORMAT='S16LE', SDL_AUDIO_FREQUENCY='48000', SDL_AUDIO_CHANNELS='2',
                   ALSA_CONFIG_PATH=str(self.config))
        return env

    def _read(self):
        pending = b''
        started = None
        try:
            with self.path.open('wb', buffering=0) as output, wave.open(output, 'wb') as recording:
                recording.setparams((2, 2, 48000, 0, 'NONE', 'not compressed'))
                while True:
                    ready, _, _ = select.select([self.fd], [], [], .05)
                    chunk = os.read(self.fd, 4096) if ready else b''
                    if not chunk:
                        if self.stop.is_set():
                            break
                        time.sleep(.01)
                        continue
                    if started is None:
                        started = time.monotonic()
                    pending += chunk
                    size = len(pending) & ~3
                    if size:
                        recording.writeframesraw(pending[:size])
                        self.frames += size // 4
                        pending = pending[size:]
                        self.stop.wait(max(0, started + self.frames / 48000 - time.monotonic()))
                if pending:
                    raise RuntimeError('Native ALSA output ended with a partial stereo sample')
        except Exception as error:
            self.error = repr(error)
        finally:
            if self.fd is not None:
                os.close(self.fd)
                self.fd = None

    def close(self):
        # Call only after the owned XEMU process has exited, so all remaining
        # FIFO bytes can be drained and the real WAV header finalized.
        self.stop.set()
        if self.thread:
            self.thread.join(timeout=5)
            if self.thread.is_alive():
                self.error = self.error or 'Native capture reader did not finish'
        elif self.fd is not None:
            os.close(self.fd)
            self.fd = None
        self.pipe.unlink(missing_ok=True)


def build(folder, phase):
    with (folder / f'{phase}-build.log').open('wb') as log:
        subprocess.run(xbox_build_command('--repack'), cwd=ROOT,
                       env=dict(os.environ, MSYSTEM='CLANG64'),
                       stdout=log, stderr=subprocess.STDOUT, check=True)


def replay_frames(payload):
    sizes = {b'RFI2': 28, b'RFI3': 32, b'RFI4': 40, b'RFI5': 44, b'RFI6': 48}
    offset = 8 if payload[:4] in sizes else 0
    size = sizes.get(payload[:4], 24)
    if offset and (len(payload) < 8 or struct.unpack_from('<I', payload, 4)[0] != size):
        raise ValueError('Malformed ordinary replay header')
    if len(payload) <= offset or (len(payload) - offset) % size:
        raise ValueError('Malformed ordinary replay length')
    return (len(payload) - offset) // size


def inspect_wave(path, scene_start, game_path):
    """Inspect only the emulator-produced recording; never synthesize samples."""
    with wave.open(str(path), 'rb') as recording:
        channels, width, rate, frames = (recording.getnchannels(), recording.getsampwidth(),
                                        recording.getframerate(), recording.getnframes())
        if (channels, width, rate, recording.getcomptype()) != (2, 2, 48000, 'NONE'):
            raise RuntimeError('Native ALSA capture returned an unexpected PCM format')
        # The first scene observation can follow its earliest sound. Retain one
        # preceding second so the review clip includes natural startup speech.
        start = max(0, min(frames, scene_start) - rate)
        recording.setpos(start)
        samples = nonzero = peak = squares = clipped = 0
        with wave.open(str(game_path), 'wb') as game:
            game.setparams((channels, width, rate, 0, 'NONE', 'not compressed'))
            while True:
                pcm = recording.readframes(48000)
                if not pcm:
                    break
                game.writeframesraw(pcm)
                values = array('h', pcm)
                if sys.byteorder != 'little':
                    values.byteswap()
                for value in values:
                    magnitude = abs(value)
                    samples += 1
                    nonzero += value != 0
                    clipped += magnitude >= 32767
                    peak = max(peak, magnitude)
                    squares += value * value
    return dict(source=str(path), source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                game_clip=str(game_path), game_clip_sha256=hashlib.sha256(game_path.read_bytes()).hexdigest(),
                source_frames=frames, game_start_frame=start, game_frames=samples // channels,
                rate=rate, channels=channels, bits=width * 8, nonzero_samples=nonzero,
                peak=peak, rms=math.sqrt(squares / samples) if samples else 0,
                clipped_samples=clipped,
                scope='Linear XEMU SDL output captured through a paced ALSA file sink; listening/intelligibility review remains separate.')


def capture_guest(folder, hdd, frames, seconds, lock):
    emulator = emulator_root()
    shutil.copyfile(emulator / 'eeprom.bin', folder / 'eeprom.bin')
    capture = SDLAudioCapture(folder)
    wav_path = capture.path
    config = folder / 'xemu.toml'
    config.write_text('''[general]
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
''' + '\n'.join(f'{key} = {json.dumps(str(value))}' for key, value in {
        'bootrom_path': emulator / 'MCPX/mcpx_1.0.bin',
        'flashrom_path': emulator / 'BIOS/xbox-4627_debug.bin',
        'eeprom_path': folder / 'eeprom.bin', 'hdd_path': hdd,
        'dvd_path': ROOT / 'build/xbox/redfaction-diagnostic.iso',
    }.items()) + '\n')
    pipe_qmp = os.name != 'nt'
    port = None
    if not pipe_qmp:
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
    # This XEMU APU uses SDL3 directly, bypassing QEMU audiodevs. The process-
    # local ALSA capture below receives that real output; -audio none only
    # disables unused QEMU sound devices and is not the Xbox output path.
    command = [emulator_binary(emulator), '-config_path', str(config), '-m', '64',
               '-snapshot', '-display', 'xemu', '-audio', 'none',
               '-qmp', 'stdio' if pipe_qmp else f'tcp:127.0.0.1:{port},server=on,wait=off']
    mapping = (ROOT / 'build/xbox/main.map').read_text()
    symbols = {name: address(mapping, name) for name in COUNTS}
    result = dict(command=command, samples=[], source_commit=subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        xbe_sha256=hashlib.sha256((DISC / 'default.xbe').read_bytes()).hexdigest(),
        map_sha256=hashlib.sha256(mapping.encode()).hexdigest(),
        audio_backend=dict(kind='SDL3/ALSA file PCM', config=str(capture.config),
                           pcm='rf_native_capture', rate=48000, channels=2, format='S16_LE'))
    monitor = process = None
    scene_start = None
    try:
        require_no_project_xemu(ROOT)
        capture.start()
        result['audio_backend']['fifo_bytes'] = capture.pipe_bytes
        startup = None
        if os.name == 'nt':
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with (folder / 'stdout.log').open('wb') as out, (folder / 'stderr.log').open('wb') as err:
            process = subprocess.Popen(command, cwd=folder, env=capture.environment(folder),
                stdin=subprocess.PIPE if pipe_qmp else None,
                stdout=subprocess.PIPE if pipe_qmp else out, stderr=err, startupinfo=startup,
                pass_fds=lock.inherited_fds(),
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            deadline = time.monotonic() + seconds
            last = None
            while time.monotonic() < deadline:
                if capture.error:
                    raise RuntimeError('Native audio capture failed: ' + capture.error)
                if process.poll() is not None:
                    raise RuntimeError(f'XEMU exited {process.returncode}; see stderr.log')
                if monitor is None:
                    try:
                        monitor = PipeMonitor(process, out) if pipe_qmp else Monitor(port)
                        if pipe_qmp:
                            monitor.negotiate()
                    except OSError:
                        if pipe_qmp:
                            raise
                        time.sleep(.5)
                        continue
                    result['memory'] = monitor.command('query-memory-size-summary')
                    if result['memory'].get('base-memory') != 64 * 1024 * 1024:
                        raise RuntimeError('Require stock 64 MiB Xbox memory')
                    result['unused_qemu_audiodevs'] = monitor.command('query-audiodevs')
                try:
                    diagnostic = words(monitor, symbols['rf_diagnostic'], 58)
                except RuntimeError as exc:
                    if 'received 0' not in str(exc):
                        raise
                    time.sleep(.5)
                    continue
                if diagnostic[0] != 0x52464447:
                    time.sleep(.5)
                    continue
                if diagnostic[2] == 2 and diagnostic[37] > 0:
                    wave_frames = capture.frames
                    if scene_start is None:
                        scene_start = wave_frames
                    row = dict(frame=diagnostic[37], wave_frames_written=wave_frames,
                               music=words(monitor, symbols['rf_xbox_music_diagnostic'], 12),
                               speech=words(monitor, symbols['rf_scene_message_playback'], 16))
                    result['samples'].append(row)
                stage = (diagnostic[2], diagnostic[37] // 60)
                if stage != last:
                    print(f'Native audio: phase {diagnostic[2]}, frame {diagnostic[37]}', flush=True)
                    last = stage
                if diagnostic[2] == 5 or diagnostic[2] & 0x80000000:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError(f'Native audio interval did not finish in {seconds}s')
            monitor.command('stop')
            result['terminal'] = {name: words(monitor, symbols[name], count)
                                  for name, count in COUNTS.items()}
            result['scene_start_wave_frame'] = scene_start
    finally:
        try:
            if monitor:
                try:
                    monitor.command('quit')
                except (OSError, RuntimeError):
                    pass
        finally:
            try:
                stop_owned_process(process)
            finally:
                try:
                    if monitor:
                        monitor.close()
                finally:
                    capture.close()
                    result['audio_backend']['captured_frames'] = capture.frames
                    result['audio_backend']['capture_error'] = capture.error
                    (folder / 'guest-result.json').write_text(json.dumps(result, indent=2) + '\n')
    if capture.error:
        raise RuntimeError('Native audio capture failed: ' + capture.error)
    if not result['terminal']['rf_player_replay_diagnostic'][0]:
        raise RuntimeError('Xbox did not admit the ordinary replay; inspect scene-preview/player-control staging')
    if scene_start is None:
        raise RuntimeError('No live original scene was observed; recording cannot establish game output')
    result['recording'] = inspect_wave(wav_path, scene_start, folder / 'native-game.wav')
    (folder / 'guest-result.json').write_text(json.dumps(result, indent=2) + '\n')
    terminal = result['terminal']
    if terminal['rf_diagnostic'][2] != 5 or terminal['rf_diagnostic'][37] != frames:
        raise RuntimeError('Native interval did not complete cleanly')
    if terminal['rf_xbox_level_transitions'][0]:
        raise RuntimeError('Original audio interval unexpectedly left its level')
    if not 0 < terminal['rf_diagnostic'][44] <= 16384:
        raise RuntimeError('Stock-memory availability was not valid at completion')
    if terminal['rf_player_replay_diagnostic'][2] != frames:
        raise RuntimeError('Ordinary process-local input was not fully consumed')
    audio = terminal['rf_xbox_audio_diagnostic']
    if audio[3] or audio[11] or not audio[4]:
        raise RuntimeError(f'Native audio failed or did not reset cleanly: {audio}')
    recording = result['recording']
    if recording['game_frames'] < 48000 or not recording['nonzero_samples'] or recording['peak'] < 16:
        raise RuntimeError('Native game output was absent, empty, or effectively silent')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--seconds', type=int, default=300)
    parser.add_argument('--spawn-only', action='store_true',
                        help='Record natural startup VO at original spawn, without music-trigger placement')
    parser.add_argument('--input', type=Path, help='Existing ordinary replay; no host input is sent')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    payload = args.input.read_bytes() if args.input else b'RFI6' + struct.pack('<I', 48) + bytes(args.frames * 48)
    frames = replay_frames(payload)
    if not 120 <= frames <= 1800 or not 30 <= args.seconds <= 900:
        parser.error('Require 120..1800 ordinary frames and 30..900 wall-clock seconds')
    hdd = ROOT / 'local/xemu-harness/pacing-base.qcow2'
    if not hdd.is_file():
        raise RuntimeError('Missing owned test HDD base; no user HDD is substituted')
    folder = args.out.resolve() if args.out else ROOT / 'artifacts/xemu' / (
        'native-audio-' + datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'))
    lock = SessionLock(ROOT)
    original = None
    report = dict(result='FAIL', frames=frames, level='L1S1.rfl',
                  trigger_placement=None if args.spawn_only else 9917,
                  input_sha256=hashlib.sha256(payload).hexdigest(),
                  scope='Native output recording of ordinary original triggers; no injected event, PC run, image, or host input.',
                  listening_review='Pending; nonzero native PCM is not an intelligibility/quality verdict.')
    try:
        lock.acquire()
        require_no_project_xemu(ROOT)
        folder.mkdir(parents=True, exist_ok=False)
        names = set(FLAGS) | {p.name for p in DISC.glob('campaign-*') if p.is_file()}
        names |= {p.name for p in DISC.glob('*.flag') if p.is_file()}
        names |= {'scene-preview.flag', 'player-control.flag', 'audio-output.flag', 'particle-step-fixtures.bin'}
        original = {name: (DISC / name).read_bytes() if (DISC / name).exists() else None for name in sorted(names)}
        (folder / 'disc-restore.json').write_text(json.dumps({
            name: data.hex() if data is not None else None for name, data in original.items()}, indent=2) + '\n')
        for name in original:
            (DISC / name).unlink(missing_ok=True)
        (DISC / 'campaign-spawn.flag').write_bytes(b'')
        (DISC / 'campaign-level.bin').write_bytes(b'levels1.vpp'.ljust(64, b'\0') + b'L1S1.rfl'.ljust(64, b'\0'))
        if not args.spawn_only:
            (DISC / 'campaign-trigger-start.bin').write_bytes(struct.pack('<I', 9917))
        # main() enters the live scene only through this outer admission flag.
        # player-control alone is never read by the three-frame static preview.
        (DISC / 'scene-preview.flag').write_bytes(b'')
        (DISC / 'player-control.flag').write_bytes(b'')
        (DISC / 'audio-output.flag').write_bytes(b'')
        (DISC / 'player-replay.bin').write_bytes(payload)
        report['staged_flags'] = sorted(p.name for p in DISC.glob('*.flag') if p.is_file())
        report['staged_replay_bytes'] = (DISC / 'player-replay.bin').stat().st_size
        build(folder, 'audio')
        guest = capture_guest(folder, hdd, frames, args.seconds, lock)
        report['guest'] = guest
        terminal = guest['terminal']
        speech = terminal['rf_scene_message_audio']
        playback = terminal['rf_scene_message_playback']
        if not speech[1] or speech[2] or not playback[11]:
            raise RuntimeError(f'Natural startup VO was not observed on its native device clock: {speech}, {playback}')
        if not args.spawn_only:
            scene_music = terminal['rf_scene_music']
            music = terminal['rf_xbox_music_diagnostic']
            if scene_music[0] < 1 or scene_music[4] != 9913 or scene_music[2] or music[2] <= 2 or not music[3] or music[5] or not music[7]:
                raise RuntimeError(f'Original music trigger did not feed native output: {scene_music}, {music}')
        report['result'] = 'PASS_NATIVE_PCM'
    except Exception as error:
        report['error'] = repr(error)
        raise
    finally:
        try:
            if original is not None:
                for name, data in original.items():
                    if data is None:
                        (DISC / name).unlink(missing_ok=True)
                    else:
                        (DISC / name).write_bytes(data)
                build(folder, 'restore')
                report['disc_restored'] = all(((DISC / name).read_bytes() if (DISC / name).exists() else None) == data
                                            for name, data in original.items())
                if not report['disc_restored']:
                    report['result'] = 'FAIL'
                    raise RuntimeError('Native audio disc flags were not restored')
        except Exception as error:
            report['result'] = 'FAIL'
            report['restore_error'] = repr(error)
            raise
        finally:
            lock.close()
            if original is not None:
                (folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
                print(folder, report['result'], flush=True)


if __name__ == '__main__':
    main()
