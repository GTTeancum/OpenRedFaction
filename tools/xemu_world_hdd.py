"""One reusable, private ordinary-save HDD; never writes the harness base."""
import json
import shutil
import struct
from pathlib import Path


def standalone(path):
    with path.open('rb') as f:
        h = f.read(24)
    if (len(h) != 24 or h[:4] != b'QFI\xfb' or
            struct.unpack_from('>I', h, 4)[0] not in (2, 3) or
            struct.unpack_from('>Q', h, 8)[0] or struct.unpack_from('>I', h, 16)[0]):
        raise ValueError('Require standalone QCOW2 without a backing chain: ' + str(path))


def prepare(root, base):
    folder = Path(root).resolve() / 'artifacts/ordinary-save-hdd'
    folder.mkdir(parents=True, exist_ok=True)
    disk = folder / 'save-test.qcow2'
    marker = folder / 'owner.json'
    owner = {'purpose': 'ordinary-save-test', 'base': str(base.resolve()), 'disk': str(disk)}
    if disk.exists():
        if not marker.exists() or json.loads(marker.read_text()) != owner:
            raise ValueError('Existing ordinary test HDD has no matching ownership record')
        standalone(disk)
        return disk
    if marker.exists():
        raise ValueError('Orphaned ordinary HDD ownership record')
    standalone(base)
    if shutil.disk_usage(folder).free < base.stat().st_size + 2 * 1024**3:
        raise RuntimeError('Insufficient space for one private HDD plus 2 GiB reserve')
    shutil.copyfile(base, disk)
    standalone(disk)
    marker.write_text(json.dumps(owner, indent=2) + '\n')
    return disk
