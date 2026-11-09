"""Copy the movies both releases share from the user's PS2 disc image
(docs/FMV-PLAN.md): THQLOGO, INTRO and ENDING .PSS, unchanged, into
an output folder as lower-case names (the Xbox disc's movies folder). The
trailers (PS2-only, or adverts for other games) are not copied. Nothing is
converted.
  python tools/extract_ps2_movies.py <ps2 iso> <output folder>"""
import os, struct, sys

MOVIES = ('THQLOGO.PSS', 'INTRO.PSS', 'ENDING.PSS')

def records(iso):
    """Yield (name, extent, size) for every file of the ISO 9660 tree."""
    f = open(iso, 'rb')
    f.seek(16 * 2048); pvd = f.read(2048)
    if pvd[1:6] != b'CD001': raise SystemExit('not an ISO 9660 image')
    root = pvd[156:156 + 34]
    pending = [(struct.unpack('<I', root[2:6])[0], struct.unpack('<I', root[10:14])[0])]
    seen = set()
    while pending:
        extent, size = pending.pop()
        if extent in seen: continue
        seen.add(extent); f.seek(extent * 2048); data = f.read(size); i = 0
        while i < len(data):
            length = data[i]
            if not length: i = (i // 2048 + 1) * 2048; continue
            rec = data[i:i + length]
            ext, sz, flags, nlen = struct.unpack('<I', rec[2:6])[0], struct.unpack('<I', rec[10:14])[0], rec[25], rec[32]
            name = rec[33:33 + nlen]
            if name not in (b'\0', b'\1'):
                if flags & 2: pending.append((ext, sz))
                else: yield name.split(b';')[0].decode('ascii', 'replace').upper(), ext, sz
            i += length

def main():
    iso, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    found = {}
    for name, ext, size in records(iso):
        if name in MOVIES: found[name] = (ext, size)
    missing = [m for m in MOVIES if m not in found]
    if missing: raise SystemExit('missing on the PS2 disc: ' + ', '.join(missing))
    with open(iso, 'rb') as f:
        for name in MOVIES:
            ext, size = found[name]
            target = os.path.join(out, name.lower())
            if os.path.exists(target) and os.path.getsize(target) == size: continue
            f.seek(ext * 2048); left = size
            with open(target + '.part', 'wb') as o:
                while left:
                    chunk = f.read(min(left, 1 << 20)); o.write(chunk); left -= len(chunk)
            os.replace(target + '.part', target)
            print(name.lower(), size)

if __name__ == '__main__':
    main()
