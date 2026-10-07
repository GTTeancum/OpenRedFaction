"""Host-only build and pipe-QMP support for bounded Xbox checks.

Linux defaults to SDL offscreen rendering. This module never captures images
or sends host input; guest memory telemetry remains the harness's interface.
"""
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time


def xbox_build_command(*arguments):
    shell = os.environ.get('RF_BASH', 'C:/msys64/usr/bin/bash.exe' if os.name == 'nt' else 'bash')
    return [shell, '--noprofile', '--norc', 'tools/build-xbox.sh', *arguments]


def emulator_root():
    return Path(os.environ.get('RF_XEMU_ROOT', 'C:/Games/Emulators/Xemu' if os.name == 'nt' else 'local/xemu')).resolve()


def emulator_binary(root):
    return os.environ.get('RF_XEMU_BINARY', str(root / ('xemu.exe' if os.name == 'nt' else 'xemu')))


def emulator_environment(folder):
    env = dict(os.environ, SDL_AUDIO_DRIVER='dummy', SDL_AUDIODRIVER='dummy')
    if os.name != 'nt':
        for name, suffix in [('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache')]:
            path = (folder / suffix).resolve()
            path.mkdir(parents=True, exist_ok=True)
            env[name] = str(path)
        env.setdefault('SDL_VIDEODRIVER', 'offscreen')
        env.setdefault('LIBGL_ALWAYS_SOFTWARE', '1')
    return env


class PipeMonitor:
    """QMP over this process's pipes, with a bounded response wait."""
    def __init__(self, process, output_log, timeout=30):
        self.process = process
        self.timeout = timeout
        self.responses = queue.Queue()
        # The caller's log context may close before its outer process teardown.
        self.output_log = os.fdopen(os.dup(output_log.fileno()), 'wb')
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def negotiate(self):
        # The caller retains this object before any handshake can fail.
        greeting = self.receive()
        if 'QMP' not in greeting:
            raise RuntimeError('Missing QMP greeting')
        self.command('qmp_capabilities')

    def _read(self):
        try:
            for line in self.process.stdout:
                self.output_log.write(line)
                self.output_log.flush()
                try:
                    value = json.loads(line)
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                if isinstance(value, dict):
                    self.responses.put(value)
        finally:
            self.output_log.close()
            self.responses.put(None)

    def receive(self, timeout=None):
        try:
            value = self.responses.get(timeout=self.timeout if timeout is None else max(0, timeout))
        except queue.Empty as exc:
            raise TimeoutError('QMP response timed out') from exc
        if value is None:
            raise RuntimeError('QMP disconnected')
        return value

    def command(self, command, arguments=None):
        deadline = time.monotonic() + self.timeout
        request = {'execute': command}
        if arguments is not None:
            request['arguments'] = arguments
        self.process.stdin.write(json.dumps(request).encode() + b'\n')
        self.process.stdin.flush()
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('QMP response timed out')
            response = self.receive(remaining)
            if 'error' in response:
                raise RuntimeError(response['error'])
            if 'return' in response:
                return response['return']

    def close(self):
        if self.process.stdin:
            try:
                self.process.stdin.close()
            except OSError:
                pass
        self.reader.join(timeout=2)
        if not self.reader.is_alive():
            self.process.stdout.close()


def stop_owned_process(process, timeout=8):
    """Reap only the subprocess returned by this harness's own Popen call."""
    if process is None:
        return
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=timeout)


class SessionLock:
    """Serialize Linux harnesses even inside separate PID namespaces."""
    def __init__(self, root):
        self.path = Path(root) / 'local/xemu-harness/session.lock'
        self.file = None

    def acquire(self):
        if os.name == 'nt':
            return
        import fcntl
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.file = self.path.open('a+')
        try:
            fcntl.flock(self.file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            self.file.close()
            self.file = None
            raise RuntimeError('An existing project XEMU harness owns the session; left untouched') from exc

    def close(self):
        if self.file:
            self.file.close()
            self.file = None

    def inherited_fds(self):
        # Keep exclusion until XEMU exits even if its harness dies abruptly.
        return (self.file.fileno(),) if self.file else ()
