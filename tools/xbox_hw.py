"""Xbox debug-kit transport for the crash harness (xbdm, TCP 731).

Stages the private crash disc folder on the console hard drive, launches it,
and reads the game's opt-in RAM log mirror with getmem. It supplies no
gameplay input: replay input stays in the staged disc files, as under XEMU.
"""
import hashlib
import json
import re
import socket
import struct
import time
from pathlib import Path

BS = chr(92)
PORT = 731


class Xbdm:
    def __init__(self, host, timeout=15):
        self.host = host
        self.s = socket.create_connection((host, PORT), timeout=timeout)
        self.f = self.s.makefile('rb')
        line = self.f.readline()
        if not line.startswith(b'201'):
            raise RuntimeError('xbdm greeting: %r' % line)

    def cmd(self, command):
        self.s.sendall(command.encode() + b'\r\n')
        line = self.f.readline().decode(errors='replace').strip()
        out = [line]
        if line.startswith('202'):
            while True:
                more = self.f.readline().decode(errors='replace').strip()
                if more == '.':
                    break
                out.append(more)
        return out

    def ok(self, command):
        reply = self.cmd(command)
        if not reply[0].startswith(('200', '202')):
            raise RuntimeError('%s -> %s' % (command, reply[0]))
        return reply

    def dirlist(self, path):
        reply = self.cmd('dirlist name="%s"' % path)
        if not reply[0].startswith('202'):
            return None
        items = {}
        for line in reply[1:]:
            m = re.match(r'name="([^"]*)" (.*)', line)
            if not m:
                continue
            kv = dict(re.findall(r'(\w+)=(\S+)', m[2]))
            size = (int(kv.get('sizehi', '0'), 16) << 32) | int(kv.get('sizelo', '0'), 16)
            items[m[1]] = (size, 'directory' in m[2])
        return items

    def getmem(self, address, length):
        data = bytearray()
        while length:
            chunk = min(length, 4096)
            reply = self.cmd('getmem addr=0x%08x length=0x%x' % (address, chunk))
            if not reply[0].startswith('202'):
                raise RuntimeError('getmem 0x%08x: %s' % (address, reply[0]))
            text = ''.join(reply[1:])
            if '?' in text or len(text) != chunk * 2:
                raise RuntimeError('getmem 0x%08x returned unreadable memory' % address)
            data += bytes.fromhex(text)
            address += chunk
            length -= chunk
        return bytes(data)

    def sendfile(self, remote, payload):
        self.s.sendall(('sendfile name="%s" length=0x%x' % (remote, len(payload))).encode() + b'\r\n')
        line = self.f.readline()
        if not line.startswith(b'204'):
            raise RuntimeError('sendfile %s: %r' % (remote, line))
        self.s.sendall(payload)
        line = self.f.readline()
        if not line.startswith(b'200'):
            raise RuntimeError('sendfile %s: %r' % (remote, line))

    def close(self):
        try:
            self.s.sendall(b'bye\r\n')
            self.s.close()
        except OSError:
            pass


def connect(host, seconds=90, timeout=15):
    deadline = time.monotonic() + seconds
    last = None
    while time.monotonic() < deadline:
        try:
            return Xbdm(host, timeout)
        except OSError as exc:
            last = exc
            time.sleep(1)
    raise RuntimeError('console %s unreachable: %s' % (host, last))


def sync_disc(host, local, remote, manifest_path):
    """Mirror the local disc folder into one console folder.

    Only this harness-owned folder is touched. Unchanged large files are
    skipped through a local size+hash manifest; files absent locally are
    removed so stale run flags cannot leak into the next run."""
    if not remote.startswith('E:' + BS) or remote.rstrip(BS).count(BS) != 1:
        raise ValueError('Console staging must be one top-level E: folder')
    try:
        manifest = json.loads(Path(manifest_path).read_text())
    except (OSError, ValueError):
        manifest = {}
    x = connect(host, timeout=600)
    sent = 0
    try:
        listing = x.dirlist(remote)
        if listing is None:
            x.ok('mkdir name="%s"' % remote.rstrip(BS))
            listing = {}
            manifest = {}
        files = {p.name: p for p in Path(local).iterdir() if p.is_file()}
        for name in listing:
            if name not in files and not listing[name][1]:
                x.ok('delete name="%s"' % (remote + name))
                manifest.pop(name, None)
        for name, path in sorted(files.items()):
            size = path.stat().st_size
            stamp = [size, path.stat().st_mtime_ns]
            known = manifest.get(name)
            if name in listing and listing[name][0] == size and known and known[:2] == stamp:
                continue
            payload = path.read_bytes()
            digest = hashlib.sha256(payload).hexdigest()
            if name in listing and listing[name][0] == size and known and known[2] == digest:
                manifest[name] = stamp + [digest]
                continue
            x.sendfile(remote + name, payload)
            manifest[name] = stamp + [digest]
            sent += size
        final = x.dirlist(remote) or {}
        for name, path in files.items():
            if final.get(name, (None,))[0] != path.stat().st_size:
                raise RuntimeError('Console copy size mismatch: ' + name)
    finally:
        x.close()
        Path(manifest_path).write_text(json.dumps(manifest))
    return sent


def launch(host, xbe):
    x = connect(host)
    try:
        x.s.sendall(('magicboot title="%s" debug' % xbe).encode() + b'\r\n')
        x.f.readline()
    except OSError:
        pass
    finally:
        x.close()
    time.sleep(3)
    return connect(host, 120)


def reboot(host):
    try:
        x = Xbdm(host)
        x.s.sendall(b'reboot\r\n')
        x.close()
    except OSError:
        pass


class HardwareReader:
    """Drop-in for the vendored reader's poll_port over xbdm getmem."""

    def __init__(self, host, decode_mirror, mirror_bytes, magic):
        self.host = host
        self.decode_mirror = decode_mirror
        self.mirror_bytes = mirror_bytes
        self.magic = magic

    def poll_port(self, _port, symbols, _args):
        x = connect(self.host, 30)
        try:
            def u32(name):
                return struct.unpack('<I', x.getmem(symbols[name], 4))[0]
            values = {
                'boot_phase': u32('g_XboxBootPhase'),
                'writes': u32('g_XboxLogWriteCount'),
                'heartbeats': u32('g_XboxHeartbeatCount'),
                'last_tick': u32('g_XboxLastLogTick'),
                'offset': u32('g_XboxLogMirrorWriteOffset'),
                'wrapped': u32('g_XboxLogMirrorWrapped'),
                'magic0': u32('g_XboxDebugMirrorMagic0'),
                'magic1': u32('g_XboxDebugMirrorMagic1'),
            }
            if (values['magic0'], values['magic1']) != self.magic:
                raise RuntimeError('RAM mirror header magic mismatch; rejecting capture')
            raw = x.getmem(symbols['g_XboxLogMirror'], self.mirror_bytes)
            text = self.decode_mirror(raw, values['offset'] or 0, values['wrapped'] or 0)
            values['clock_probe'] = {'valid': False, 'reason': 'not read over xbdm'}
            return None, values, text
        finally:
            x.close()
