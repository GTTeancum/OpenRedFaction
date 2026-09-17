"""Read a live renderer texture through QMP; no host input or GPU state changes."""
import hashlib
import struct
from xemu_guest_snapshot import words


def swizzle_rgba(linear, width, height):
    if not width or not height or width & (width-1) or height & (height-1):
        raise ValueError('Expected power-of-two dimensions')
    if len(linear) != width*height*4:
        raise ValueError('RGBA byte count')
    output = bytearray(len(linear))
    for y in range(height):
        for x in range(width):
            address = shift = 0
            for bit in range(max(width,height).bit_length()-1):
                if 1 << bit < width:
                    address |= ((x >> bit) & 1) << shift
                    shift += 1
                if 1 << bit < height:
                    address |= ((y >> bit) & 1) << shift
                    shift += 1
            at = (y*width+x)*4
            output[address*4:address*4+4] = linear[at:at+4]
    return bytes(output)


def capture(monitor, symbol, reference, output):
    data = reference.read_bytes()
    assert data[:4] == b'RFT1' and len(data) >= 24
    material,width,height,source_format,size = struct.unpack_from('<5I',data,4)
    assert source_format == 6 and size == width*height*4 and len(data) == 24+size
    # Xbox32 renderer.c gpu_texture is exactly three uint32 words.
    table = words(monitor,symbol('stream_textures'),1)[0]
    assert table, 'No live stream texture table'
    pointer,fmt,transparent = words(monitor,table+material*12,3)
    assert pointer and (fmt >> 8) & 255 == 0x3a, 'Expected swizzled A8B8G8R8'
    assert (fmt >> 16) & 15 == 1 and (fmt >> 4) & 15 == 2
    assert 1 << ((fmt >> 20) & 15) == width and 1 << ((fmt >> 24) & 15) == height
    assert transparent == 0
    physical = pointer & 0x03ffffff
    assert physical+size <= 64*1024*1024
    monitor.command('human-monitor-command', {'command-line':
        f'pmemsave 0x{physical:x} {size} "{output.as_posix()}"'})
    actual = output.read_bytes()
    expected = swizzle_rgba(data[24:],width,height)
    equal = actual == expected
    result = dict(equal=equal, material=material, width=width, height=height,
                  bytes=size, gpu_pointer=pointer, physical_address=physical,
                  format=fmt, mip_levels=1,
                  xbox_sha256=hashlib.sha256(actual).hexdigest(),
                  expected_swizzled_sha256=hashlib.sha256(expected).hexdigest(),
                  scope='Live GPU-visible allocation selected by renderer texture table; exact PC-to-Xbox swizzle and descriptor. Not a draw-command or final sampling proof.')
    assert equal, 'GPU-visible texture bytes differ from PC owner'
    return result
