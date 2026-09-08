"""Read-only input fingerprinting. No game data is copied into tracked source."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def pe(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError(f'{path}: missing MZ header')
    offset, = struct.unpack_from('<I', data, 0x3c)
    if data[offset:offset + 4] != b'PE\0\0':
        raise ValueError(f'{path}: missing PE signature')
    machine, count, timestamp, _, _, optional_size, _ = struct.unpack_from('<HHIIIHH', data, offset + 4)
    optional = offset + 24
    if struct.unpack_from('<H', data, optional)[0] != 0x10b:
        raise ValueError('Expected PE32')
    entry, = struct.unpack_from('<I', data, optional + 16)
    base, = struct.unpack_from('<I', data, optional + 28)
    image_size, = struct.unpack_from('<I', data, optional + 56)
    sections = []
    for index in range(count):
        pos = optional + optional_size + index * 40
        name, virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<8sIIII', data, pos)
        sections.append(dict(name=name.rstrip(b'\0').decode('ascii'), virtual_size=virtual_size,
                             rva=rva, raw_size=raw_size, raw_offset=raw_offset))
    return dict(machine=hex(machine), timestamp=timestamp, image_base=hex(base),
                entry_point=hex(base + entry), image_size=image_size, sections=sections)


def archive(path):
    length = path.stat().st_size
    with path.open('rb') as stream:
        magic, version, count, declared_size = struct.unpack('<4I', stream.read(16))
        if magic != 0x51890ace or version != 1 or count > 65536 or declared_size != length:
            raise ValueError(f'{path}: unsupported or malformed VPP header')
        cursor = 2048 + ((count * 64 + 2047) // 2048) * 2048
        if cursor > length:
            raise ValueError(f'{path}: directory outside archive')
        stream.seek(2048)
        entries = []
        for _ in range(count):
            name, size = struct.unpack('<60sI', stream.read(64))
            if b'\0' not in name or cursor + size > length:
                raise ValueError(f'{path}: invalid entry')
            entries.append(dict(name=name.split(b'\0')[0].decode('cp1252'), size=size, offset=cursor))
            cursor += ((size + 2047) // 2048) * 2048
        if cursor > length:
            raise ValueError(f'{path}: padding outside archive')
    return dict(version=version, entries=entries, payload_bytes=sum(e['size'] for e in entries))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('game_directory', type=Path)
    parser.add_argument('--output', type=Path, default=Path('artifacts/inventory.json'))
    args = parser.parse_args()
    root = args.game_directory.resolve(strict=True)
    if not (root / 'RF.exe').is_file():
        parser.error('Game directory must contain RF.exe')
    records = []
    for path in sorted(root.rglob('*')):
        if not path.is_file():
            continue
        item = dict(path=path.relative_to(root).as_posix(), bytes=path.stat().st_size, sha256=digest(path))
        if path.suffix.lower() == '.vpp':
            item['vpp'] = archive(path)
        if path.name.lower() == 'rf.exe':
            item['pe'] = pe(path)
        records.append(item)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(root=str(root), files=records), indent=2), encoding='utf-8')
    archives = [r for r in records if 'vpp' in r]
    print(f'{len(records)} files, {sum(r["bytes"] for r in records)} bytes; '
          f'{len(archives)} VPPs, {sum(len(r["vpp"]["entries"]) for r in archives)} entries')
    print(args.output.resolve())


if __name__ == '__main__':
    main()
