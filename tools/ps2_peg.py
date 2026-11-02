"""Decode Red Faction PS2 .peg texture sheets (SLUS-20073) into RGBA images.

Reads only the user's own PS2 disc data; writes nothing by itself.

PEG v6 layout (reverse-engineered from the disc files; field meanings are
inferred from sizes and decoded output, not from the PS2 executable):
  header  0x00 'GEKV', 0x04 version 6, 0x08 header size, 0x0c data size,
          0x10 entry count, 0x18 entry count again, 0x1c 16
  entry   0x20 + 0x40*i: u16 width, u16 height, u8 format, u8 palette,
          u8 ?, u8 frames, u8 ?, u8 mips, u16 ?, char name[48], u32 offset
Formats:
  4  8-bit indexed, 256-entry palette first: palette 1 = RGBA5551 (512 bytes),
     palette 2 = RGBA8888 (1024 bytes); PS2 alpha 0x80 = opaque. Palette order
     is the GS CSM1 layout (entries 8-15 and 16-23 of each 32 swapped).
  2  opaque picture stored as MPEG-2 intra frames for the PS2 IPU: a 16-byte
     header, then one MPEG sequence per tile, tiles row-major in 256-pixel
     columns (the last column narrower), each frame cropped to its tile.
"""
import re
import struct


def entries(data):
    if data[:4] != b'GEKV' or struct.unpack_from('<I', data, 4)[0] != 6:
        raise ValueError('not a PEG v6 file')
    count = struct.unpack_from('<I', data, 0x10)[0]
    result = []
    for i in range(count):
        e = data[0x20 + i * 0x40:0x60 + i * 0x40]
        w, h = struct.unpack_from('<HH', e)
        result.append(dict(width=w, height=h, format=e[4], palette=e[5], frames=e[7],
                           name=e[12:60].split(b'\0')[0].decode('latin1'),
                           offset=struct.unpack_from('<I', e, 60)[0]))
    ordered = sorted(result, key=lambda x: x['offset'])
    for i, x in enumerate(ordered):
        x['end'] = ordered[i + 1]['offset'] if i + 1 < len(ordered) else len(data)
    return result


def _csm1(index):
    return (index & ~0x18) | ((index & 0x08) << 1) | ((index & 0x10) >> 1)


def _alpha(a):
    return min(255, a * 2)


def _indexed(data, entry):
    w, h, o = entry['width'], entry['height'], entry['offset']
    if entry['palette'] == 2:
        raw = [struct.unpack_from('<4B', data, o + i * 4) for i in range(256)]
        pal = [(r, g, b, _alpha(a)) for r, g, b, a in raw]
        o += 1024
    elif entry['palette'] == 1:
        pal = []
        for i in range(256):
            v = struct.unpack_from('<H', data, o + i * 2)[0]
            pal.append(((v & 31) * 255 // 31, (v >> 5 & 31) * 255 // 31, (v >> 10 & 31) * 255 // 31,
                        255 if v & 0x8000 else 0))
        o += 512
    else:
        raise ValueError('unsupported palette %d in %s' % (entry['palette'], entry['name']))
    pal = [pal[_csm1(i)] for i in range(256)]
    pixels = data[o:o + w * h]
    if len(pixels) != w * h:
        raise ValueError('truncated %s' % entry['name'])
    out = bytearray(w * h * 4)
    for i, p in enumerate(pixels):
        out[i * 4:i * 4 + 4] = bytes(pal[p])
    return bytes(out)


def _mpeg(data, entry):
    import av
    from PIL import Image
    w, h = entry['width'], entry['height']
    blob = data[entry['offset'] + 16:entry['end']]
    starts = [m.start() for m in re.finditer(b'\x00\x00\x01\xb3', blob)]
    canvas = Image.new('RGBA', (w, h))
    x = y = row_h = 0
    for i, s in enumerate(starts):
        chunk = blob[s:starts[i + 1] if i + 1 < len(starts) else len(blob)]
        codec = av.CodecContext.create('mpeg2video', 'r')
        frames = []
        for packet in codec.parse(chunk + b'\0\0\1\xb7'):
            frames += codec.decode(packet)
        frames += codec.decode(None)
        if len(frames) != 1:
            raise ValueError('tile %d of %s decoded %d frames' % (i, entry['name'], len(frames)))
        tile = frames[0].to_image().convert('RGBA')
        if x >= w:
            x, y = 0, y + row_h
        canvas.paste(tile, (x, y))
        x += tile.width
        row_h = tile.height
    return canvas.tobytes()


def decode(data, entry):
    """RGBA bytes (width*height*4) for one entry's first frame."""
    if entry['format'] == 4:
        return _indexed(data, entry)
    if entry['format'] == 2:
        return _mpeg(data, entry)
    raise ValueError('unsupported format %d in %s' % (entry['format'], entry['name']))


if __name__ == '__main__':
    import sys
    from pathlib import Path
    from PIL import Image
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    data = Path(sys.argv[1]).read_bytes()
    for e in entries(data):
        try:
            Image.frombytes('RGBA', (e['width'], e['height']), decode(data, e)).save(out / (Path(e['name']).stem + '.png'))
        except ValueError as exc:
            print('skip', exc)
