"""Build the Xbox PS2-menu pack (port-owned output, never committed).

Reads the user's own Red Faction PS2 disc image (SLUS-20073, "Red Faction
(USA) (v2.00).iso") and writes build/data/ps2menu.vpp, a VPP v1 archive the
Xbox front end reads instead of the PC menu art:

  <picture>.tga    every picture of the PS2 menu sheets, 32-bit uncompressed
                   TGA (the PC game's own image format), decoded from the
                   disc's PEG sheets by tools/ps2_peg.py
  plan-NNNN.dds    the animated main-menu backdrop (interface-bg-mm.peg),
                   resized to 512x256 and compressed DXT1 (Xbox-native, read
                   by rf_image_dds); every frame is kept
  *.vf             PS2 fonts; same VFNT layout as the PC fonts
  *_2x.tga/.fnt    720p menu text: the PS2 fonts' characters rendered from a
                   free TrueType font (Orbitron Bold, SIL OFL; downloaded to
                   local/fonts/orbitron) at twice the PS2 size, glyph strip
                   TGA plus metrics
  strings.tbl      PS2 menu text; ps2_multi.tbl multiplayer screen lists

Usage: build_ps2_menu_pack.py <ps2 iso> <output.vpp> [menu font .ttf]
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_peg  # noqa: E402

SHEETS = ('interface-common.peg', 'interface-frontend.peg', 'interface-backend.peg',
          'interface-startup.peg', 'interface-cardcheck.peg', 'interface-multi.peg',
          'interface-endgame.peg', 'memcardbg00.peg', 'transitionbg00.peg',
          'loadingbg00a.peg', 'loadingbg01a.peg', 'loadingbg02a.peg',
          'loadingbg03a.peg', 'loadingbg04a.peg', 'loadingbg05a.peg')
BACKDROP = 'interface-bg-mm.peg'
FONTS = ('bigfont.vf', 'font-small.vf', 'font01.vf', 'font02.vf', 'message.vf',
         'smallfont.vf', 'standard.vf', 'title.vf')
TABLES = ('strings.tbl', 'ps2_multi.tbl')
# PS2 arenas built from PC multiplayer maps (respawn points identical or the
# majority shared): their authored bot paths (RFL section 0x20000, same layout
# in PS2 v175 and PC v180) are packed as <pc map>.nav and their bot
# characters (section 0x30000) as <pc map>.ent and their named waypoint lists
# (section 0x10000) as <pc map>.wpt for the Xbox bots.
PS2_ARENAS = {'m_ps201.rfl': 'dm01', 'm_ps202.rfl': 'dm02', 'm_ps203.rfl': 'dm03',
              'm_ps204.rfl': 'dm04', 'm_ps205.rfl': 'dm05', 'm_ps207.rfl': 'dm06',
              'm_ps208.rfl': 'dm07', 'm_ps210.rfl': 'ctf02'}


def rfl_section(data, kind):
    """Payload of one RFL section; the PS2 v175 header length differs, so the
    section chain is located by walking from each candidate start."""
    count = struct.unpack_from('<I', data, 20)[0]
    for start in range(20, 200):
        pos, found = start, None
        try:
            for _ in range(count):
                t, size = struct.unpack_from('<II', data, pos)
                if t == 0 or pos + 8 + size > len(data):
                    raise ValueError
                if t == kind:
                    found = data[pos + 8:pos + 8 + size]
                pos += 8 + size
            if struct.unpack_from('<II', data, pos) == (0, 0):
                return found
        except (struct.error, ValueError):
            continue
    return None
BACKDROP_SIZE = (512, 256)


# ---- ISO 9660 (read only) ----
def iso_files(iso):
    def directory(lba, size):
        iso.seek(lba * 2048)
        data = iso.read(size)
        pos = 0
        while pos < len(data):
            length = data[pos]
            if not length:
                pos = (pos // 2048 + 1) * 2048
                continue
            rec = data[pos:pos + length]
            extent, = struct.unpack_from('<I', rec, 2)
            fsize, = struct.unpack_from('<I', rec, 10)
            flags = rec[25]
            name = rec[33:33 + rec[32]].decode('latin1').split(';')[0]
            if name not in ('\0', '\1'):
                yield name, extent, fsize, bool(flags & 2)
            pos += length
    iso.seek(16 * 2048)
    pvd = iso.read(2048)
    if pvd[1:6] != b'CD001':
        raise ValueError('not an ISO 9660 image')
    root = pvd[156:190]
    out = {}

    def walk(lba, size, prefix):
        for name, extent, fsize, is_dir in directory(lba, size):
            path = prefix + name
            if is_dir:
                walk(extent, fsize, path + '/')
            else:
                out[path.upper()] = (extent, fsize)
    walk(struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0], '')
    return out


def iso_read(iso, files, path):
    extent, size = files[path.upper()]
    iso.seek(extent * 2048)
    return iso.read(size)


# ---- VPP v1 ----
def vpp_entries(data):
    magic, version, count, _ = struct.unpack_from('<4I', data)
    if magic != 0x51890ACE or version != 1:
        raise ValueError('not a VPP v1 archive')
    offset = 2048 + (count * 64 + 2047) // 2048 * 2048
    out = {}
    for i in range(count):
        e = data[2048 + i * 64:2048 + i * 64 + 64]
        name = e[:60].split(b'\0')[0].decode('latin1')
        size, = struct.unpack_from('<I', e, 60)
        out[name.lower()] = data[offset:offset + size]
        offset += (size + 2047) // 2048 * 2048
    return out


def vpp_write(path, files):
    pad = lambda n: (n + 2047) // 2048 * 2048
    directory = b''.join(name.encode('latin1').ljust(60, b'\0') + struct.pack('<I', len(data))
                         for name, data in files)
    total = 2048 + pad(len(directory)) + sum(pad(len(d)) for _, d in files)
    with open(path, 'wb') as out:
        out.write(struct.pack('<4I', 0x51890ACE, 1, len(files), total).ljust(2048, b'\0'))
        out.write(directory.ljust(pad(len(directory)), b'\0'))
        for _, data in files:
            out.write(data.ljust(pad(len(data)), b'\0'))


# ---- outputs ----
def tga(width, height, rgba):
    """Uncompressed 32-bit TGA, top-left origin (descriptor 0x28)."""
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 0x28)
    bgra = bytearray(rgba)
    bgra[0::4], bgra[2::4] = rgba[2::4], rgba[0::4]
    return header + bytes(bgra)


def dxt1(rgb):
    """DXT1 blocks for an HxWx3 uint8 array (dimensions multiples of 4).
    Endpoints are each block's min/max along its luminance-weighted extremes."""
    import numpy as np
    h, w, _ = rgb.shape
    blocks = rgb.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(np.float32)
    lum = blocks @ np.array([0.299, 0.587, 0.114], np.float32)
    hi = blocks[np.arange(len(blocks)), lum.argmax(1)]
    lo = blocks[np.arange(len(blocks)), lum.argmin(1)]

    def pack(c):
        c = np.clip(np.rint(c), 0, 255).astype(np.uint32)
        return (c[:, 0] >> 3 << 11) | (c[:, 1] >> 2 << 5) | (c[:, 2] >> 3)

    c0, c1 = pack(hi), pack(lo)
    swap = c0 < c1
    c0[swap], c1[swap] = c1[swap], c0[swap]
    hi[swap], lo[swap] = lo[swap].copy(), hi[swap].copy()

    def unpack(c):
        return np.stack([(c >> 11 & 31) * 255 / 31, (c >> 5 & 63) * 255 / 63, (c & 31) * 255 / 31], 1)
    e0, e1 = unpack(c0), unpack(c1)
    palette = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], 1)
    dist = ((blocks[:, :, None, :] - palette[:, None, :, :]) ** 2).sum(3)
    index = dist.argmin(2).astype(np.uint32)
    equal = c0 == c1
    index[equal] = 0
    bits = np.zeros(len(blocks), np.uint32)
    for i in range(16):
        bits |= index[:, i] << (2 * i)
    out = np.zeros((len(blocks), 2), np.uint32)
    out[:, 0] = c0 | (c1 << 16)
    out[:, 1] = bits
    return out.astype('<u4').tobytes()


def dds_dxt1(width, height, payload):
    h = bytearray(128)
    h[0:4] = b'DDS '
    struct.pack_into('<IIIIII', h, 4, 124, 0x81007, height, width, len(payload), 0)
    struct.pack_into('<I', h, 28, 1)                  # mip levels
    h[32:36] = b'RFDX'                                # loader tag (reserved1[0])
    struct.pack_into('<II', h, 36, 0, 0)              # source format, alpha mode (opaque)
    struct.pack_into('<II', h, 76, 32, 4)             # pixel format size, FOURCC
    h[84:88] = b'DXT1'
    struct.pack_into('<I', h, 108, 0x1000)            # caps: texture
    return bytes(h) + payload


def font_2x(data, ttf):
    """Double-resolution replacement for a PS2 VFNT v1 font at 720p, rendered
    from a free TrueType font (default Orbitron Bold, SIL OFL) in the PS2
    font's character range. Sizing matches the PS2 glyphs: the TrueType 'H'
    is scaled to twice the PS2 capital height and sits on the PS2 baseline;
    each advance is the TrueType advance halved (PS2 1x units). White fill,
    dark outline. Returns (tga, metrics): metrics 'RFG2', count, first, cell
    height, then per glyph u16 x, y, w, h and the 1x advance."""
    from PIL import Image, ImageDraw, ImageFont
    u = lambda o: struct.unpack_from('<I', data, o)[0]
    if data[:4] != b'VFNT' or u(4) != 1 or u(8) != 0xfffffff0 or u(28):
        return None
    count, first, height, pixels = u(12), u(16), u(24), u(32)
    if 36 + count * 16 + pixels + 1024 != len(data):
        return None
    palette = data[len(data) - 1024:]
    csm1 = lambda i: (i & ~0x18) | ((i & 8) << 1) | ((i & 16) >> 1)
    # PS2 capital 'H': rows holding visible texels give cap height and baseline.
    advance, width, offset = struct.unpack_from('<III', data, 36 + (ord('H') - first) * 16)
    rows = [y for y in range(height) for x in range(width)
            if palette[csm1(data[36 + count * 16 + offset + y * width + x]) * 4 + 3] > 64]
    cap, base = max(rows) - min(rows) + 1, max(rows) + 1
    cell = height * 2

    def load(size):
        f = ImageFont.truetype(str(ttf), size)
        try:
            f.set_variation_by_name('Bold')
        except (OSError, ValueError):
            pass
        return f
    size = cap * 2
    for _ in range(64):
        box = load(size).getbbox('H', anchor='ls')
        if box[3] - box[1] >= cap * 2:
            break
        size += 1
    font = load(size)
    stroke = 2
    glyphs = []
    for i in range(count):
        code = first + i
        ps2_advance = struct.unpack_from('<I', data, 36 + i * 16)[0]
        if code > 126:
            # PS2-only symbols the menus draw (0xab/0xac arrows, 0xac also
            # backspace, 0xad-0xb0 the pad buttons): the PS2 glyph, doubled.
            gw, goff = struct.unpack_from('<II', data, 36 + i * 16 + 4)
            if not gw or code not in (0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0):
                glyphs.append((None, ps2_advance))
                continue
            src = Image.new('RGBA', (gw, height))
            px = []
            for k in range(gw * height):
                c = palette[csm1(data[36 + count * 16 + goff + k]) * 4:][:4]
                px.append((c[0], c[1], c[2], min(255, c[3] * 2)))
            src.putdata(px)
            glyphs.append((src.resize((gw * 2, cell), Image.NEAREST), ps2_advance))
            continue
        ch = chr(code)
        length = font.getlength(ch)
        w = int(length) + 2 * stroke + 2
        img = Image.new('RGBA', (w, cell))
        if ch.strip():
            ImageDraw.Draw(img).text((stroke + 1, base * 2), ch, font=font, anchor='ls',
                                     fill=(255, 255, 255, 255), stroke_width=stroke, stroke_fill=(0, 0, 0, 170))
        glyphs.append((img, max(1, int(round(length / 2)) + 1)))
    # First fit into rows of 1000 pixels, so the PS2 symbols fill row ends
    # instead of adding a row (the HUD set's canvas budget).
    rows = []
    placed = []
    for img, advance in glyphs:
        w = img.width if img else 0
        for r in range(len(rows)):
            if rows[r] + w + 1 <= 1000:
                break
        else:
            rows.append(0)
            r = len(rows) - 1
        placed.append((rows[r], r * (cell + 1), w, advance))
        rows[r] += w + 1
    strip = Image.new('RGBA', (1000, (len(rows) - 1) * (cell + 1) + cell))
    for (px, py, w, _), (img, _) in zip(placed, glyphs):
        if img:
            strip.paste(img, (px, py))
    metrics = b'RFG2' + struct.pack('<III', count, first, cell)
    metrics += b''.join(struct.pack('<HHHHI', px, py, w, cell if w else 0, adv) for px, py, w, adv in placed)
    return tga(strip.width, strip.height, strip.tobytes()), metrics


def vse_wav(data):
    # PS2 .vse sound effect: 24-byte header (size at +12, SPU pitch at +20,
    # 0x1000 = 48000 Hz) then PS2 SPU ADPCM, 16-byte blocks of 28 samples
    # (byte 0: shift | filter << 4). Output: 16-bit mono PCM WAV.
    size, pitch = struct.unpack_from('<I', data, 12)[0], struct.unpack_from('<I', data, 20)[0]
    rate = round(pitch * 48000 / 4096)
    coefs = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))
    s1 = s2 = 0
    pcm = bytearray()
    for o in range(24, min(len(data), 24 + size) - 15, 16):
        shift, filt = data[o] & 15, min(data[o] >> 4, 4)
        for k in range(28):
            nib = (data[o + 2 + k // 2] >> (4 * (k & 1))) & 15
            v = (nib - 16 if nib > 7 else nib) << 12
            v = (v >> shift) + ((s1 * coefs[filt][0] + s2 * coefs[filt][1] + 32) >> 6)
            v = max(-32768, min(32767, v))
            s2, s1 = s1, v
            pcm += struct.pack('<h', v)
    header = struct.pack('<4sI4s4sIHHIIHH4sI', b'RIFF', 36 + len(pcm), b'WAVE', b'fmt ', 16, 1, 1,
                         rate, rate * 2, 2, 16, b'data', len(pcm))
    return header + bytes(pcm)


def ps2_damage(text):
    # PS2 1c6e10 switches every weapon to the second value of these pairs in
    # multiplayer; 1ce890 picks the second $Damage / $Alt Damage there. One
    # line per weapon, tab separated: name, damage, damage multi, alt, alt
    # multi, then the multiplayer clip size, max ammo, AI attack range,
    # spread, alt spread, AI spread, AI alt spread, AI damage scale
    # ("-" when the weapon has no such line).
    import re
    lines = []
    for block in re.split(r'\$Name:', text.decode('latin1'))[1:]:
        name = re.match(r'\s*"([^"]*)"', block)
        if not name:
            continue
        def pair(key):
            m = re.search(r'\n\s*\$' + key + r':[ \t]*([-0-9.]+)[ \t]+([-0-9.]+)', block)
            return (m.group(1), m.group(2)) if m else None
        damage, alt = pair('Damage'), pair('Alt Damage')
        if not damage:
            continue
        alt = alt or damage
        multi = []
        for key in ('Clip Size', 'Max Ammo', 'AI Attack Range', 'Spread Degrees', 'Alt Spread Degrees',
                    'AI Spread Degrees', 'AI Alt Spread Degrees', 'AI Damage Scale'):
            v = pair(key)
            multi.append(v[1] if v else '-')
        lines.append('\t'.join([name.group(1), damage[0], damage[1], alt[0], alt[1]] + multi) + '\n')
    return ''.join(lines).encode('latin1')


def main():
    from PIL import Image
    import numpy as np
    iso_path, out_path = Path(sys.argv[1]), Path(sys.argv[2])
    # Free replacement for the menu fonts at 720p (SIL OFL), kept outside
    # tracked source with its licence: local/fonts/orbitron/.
    ttf = Path(sys.argv[3]) if len(sys.argv) > 3 else         Path(__file__).resolve().parents[1] / 'local/fonts/orbitron/Orbitron[wght].ttf'
    with iso_path.open('rb') as iso:
        files = iso_files(iso)
        archive = vpp_entries(iso_read(iso, files, 'DATA/RF_PS2.VPP'))
        tables = vpp_entries(iso_read(iso, files, 'DATA/TABLES.VPP'))
    out, seen = [], set()
    for sheet in SHEETS:
        data = archive[sheet]
        for e in ps2_peg.entries(data):
            name = e['name'].lower()
            if name == 'winner_win.vbm':
                # End-screen winner bitmap (1dc1a0): 15 frames at fps 0
                # (entry byte 8), so the PS2 only ever shows the first.
                name = 'winner_win.tga'
            if not name.endswith('.tga') or name in seen:
                continue
            seen.add(name)
            out.append((name, tga(e['width'], e['height'], ps2_peg.decode(data, e))))
    data = archive[BACKDROP]
    frames = sorted(ps2_peg.entries(data), key=lambda e: e['name'])
    for e in frames:
        image = Image.frombytes('RGBA', (e['width'], e['height']), ps2_peg.decode(data, e))
        rgb = np.asarray(image.convert('RGB').resize(BACKDROP_SIZE, Image.LANCZOS))
        out.append((Path(e['name']).stem.lower() + '.dds', dds_dxt1(*BACKDROP_SIZE, dxt1(rgb))))
    for name in FONTS:
        out.append((name, archive[name]))
        sharp = font_2x(archive[name], ttf)
        if sharp:
            stem = name[:-3]
            out.append((stem + '_2x.tga', sharp[0]))
            out.append((stem + '_2x.fnt', sharp[1]))
    for arena, pc_map in PS2_ARENAS.items():
        nav = rfl_section(archive[arena], 0x20000)
        if nav and len(nav) > 4:
            out.append((pc_map + '.nav', nav))
        # The arena's characters are its bots (PS2 2740e0 turns the first N
        # non-player entities into bots); same v180 entity record layout.
        ent = rfl_section(archive[arena], 0x30000)
        if ent and len(ent) > 4:
            out.append((pc_map + '.ent', ent))
        # Named waypoint lists (section 0x10000) the bots walk: the PS2 picks
        # one by name prefix ("weapon", "" for any) in 12e2a0. They index the
        # PS2 path nodes above, so they travel with the .nav.
        wpt = rfl_section(archive[arena], 0x10000)
        if wpt and len(wpt) > 4:
            out.append((pc_map + '.wpt', wpt))
    for name in TABLES:
        out.append((name, tables[name]))
    # PS2 weapon damage (weapons.tbl `$Damage:` / `$Alt Damage:` give single
    # player then multiplayer values; 1ce890 uses the second in multiplayer).
    # One line per weapon: name, damage, damage multi, alt, alt multi.
    out.append(('ps2_damage.txt', ps2_damage(tables['weapons.tbl'])))
    # PS2 sounds.tbl slot 0x40 "respawn.wav" (SND_RESPAWN, multiplayer) is
    # respawn.vse on the disc; the PC files have no such effect.
    out.append(('respawn.wav', vse_wav(archive['respawn.vse'])))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    vpp_write(out_path, out)
    print('%s: %d entries (%d pictures, %d backdrop frames)' % (out_path, len(out), len(seen), len(frames)))


if __name__ == '__main__':
    main()
