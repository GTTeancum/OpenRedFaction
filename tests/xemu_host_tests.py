"""Focused host transport checks; no game or emulator is launched."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from xemu_host import (PipeMonitor, SessionLock, emulator_environment,
                       stop_owned_process, xbox_build_command)


class HostTests(unittest.TestCase):
    def test_pipe_qmp_and_closed_callers_log(self):
        script = '''import json,sys
print('diagnostic preamble', flush=True)
print(json.dumps({'QMP': {}}), flush=True)
for line in sys.stdin:
    command=json.loads(line)['execute']
    print(json.dumps({'event':'STOP'}), flush=True)
    print(json.dumps({'return': {'command':command}}), flush=True)
    if command=='quit': break
'''
        process = subprocess.Popen([sys.executable, '-u', '-c', script],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        with tempfile.TemporaryFile() as log:
            monitor = PipeMonitor(process, log, timeout=2)
            monitor.negotiate()
        self.assertEqual(monitor.command('query-status'), {'command': 'query-status'})
        monitor.command('quit')
        process.wait(timeout=2)
        monitor.close()
        process.stdout.close()
        self.assertFalse(monitor.reader.is_alive())

    @unittest.skipIf(os.name == 'nt', 'Linux file lock')
    def test_lock_blocks_and_releases(self):
        with tempfile.TemporaryDirectory() as folder:
            first, second = SessionLock(folder), SessionLock(folder)
            first.acquire()
            with self.assertRaisesRegex(RuntimeError, 'existing project XEMU'):
                second.acquire()
            first.close()
            second.acquire()
            second.close()

    @unittest.skipIf(os.name == 'nt', 'Linux inherited file lock')
    def test_child_retains_session_lock(self):
        with tempfile.TemporaryDirectory() as folder:
            first, second = SessionLock(folder), SessionLock(folder)
            first.acquire()
            process = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'],
                                       pass_fds=first.inherited_fds())
            first.close()
            try:
                with self.assertRaisesRegex(RuntimeError, 'existing project XEMU'):
                    second.acquire()
            finally:
                process.terminate()
                stop_owned_process(process)
            second.acquire()
            second.close()

    def test_async_events_cannot_extend_timeout(self):
        script = '''import json,sys,time
print(json.dumps({'QMP': {}}), flush=True)
sys.stdin.readline()
print(json.dumps({'return': {}}), flush=True)
sys.stdin.readline()
while True:
    print(json.dumps({'event':'STOP'}), flush=True)
    time.sleep(.01)
'''
        process = subprocess.Popen([sys.executable, '-u', '-c', script],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        monitor = None
        try:
            with tempfile.TemporaryFile() as log:
                monitor = PipeMonitor(process, log, timeout=.15)
                monitor.negotiate()
                started = time.monotonic()
                with self.assertRaises(TimeoutError):
                    monitor.command('query-status')
                self.assertLess(time.monotonic() - started, .75)
        finally:
            process.terminate()
            stop_owned_process(process)
            if monitor:
                monitor.close()

    def test_owned_process_escalates_to_kill(self):
        process = Mock()
        process.wait.side_effect = [subprocess.TimeoutExpired('owned-xemu', 8),
                                    subprocess.TimeoutExpired('owned-xemu', 8), 0]
        stop_owned_process(process)
        process.terminate.assert_called_once_with()
        process.kill.assert_called_once_with()
        self.assertEqual(process.wait.call_count, 3)

    def test_failed_handshake_can_be_closed(self):
        process = subprocess.Popen([sys.executable, '-u', '-c',
                                    "print('{\"unexpected\": true}', flush=True)"],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        with tempfile.TemporaryFile() as log:
            monitor = PipeMonitor(process, log, timeout=.2)
            with self.assertRaisesRegex(RuntimeError, 'Missing QMP greeting'):
                monitor.negotiate()
        stop_owned_process(process)
        monitor.close()
        self.assertFalse(monitor.reader.is_alive())
        self.assertTrue(monitor.output_log.closed)
        self.assertTrue(process.stdin.closed)
        self.assertTrue(process.stdout.closed)

    @unittest.skipIf(os.name == 'nt', 'Linux environment')
    def test_offscreen_environment(self):
        with tempfile.TemporaryDirectory() as folder:
            env = emulator_environment(Path(folder))
            self.assertEqual(env['SDL_VIDEODRIVER'], os.environ.get('SDL_VIDEODRIVER', 'offscreen'))
            self.assertTrue(Path(env['XDG_DATA_HOME']).is_dir())
            self.assertTrue(Path(env['XDG_CACHE_HOME']).is_dir())
        self.assertEqual(xbox_build_command('--repack')[-2:], ['tools/build-xbox.sh', '--repack'])


if __name__ == '__main__':
    unittest.main()
