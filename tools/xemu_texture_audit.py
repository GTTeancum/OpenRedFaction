"""Read a live renderer texture through QMP; no host input or GPU state changes."""
import csv
import hashlib
import struct
from xemu_guest_snapshot import words


def morton_address(x, y, width, height):
    address = shift = 0
    for bit in range(max(width, height).bit_length()-1):
        if 1 << bit < width:
            address |= ((x >> bit) & 1) << shift
            shift += 1
        if 1 << bit < height:
            address |= ((y >> bit) & 1) << shift
            shift += 1
    return address


def compare_atlas(actual, width, height, rows):
    """Compare allocated chart texels only; unused atlas storage is unspecified."""
    if (not width or not height or width & (width-1) or height & (height-1)
            or len(actual) != width*height*2):
        raise ValueError('Invalid packed atlas dimensions or size')
    occupied = set()
    for row in rows:
        x, y = int(row['atlas_x']), int(row['atlas_y'])
        w, h = int(row['width']), int(row['height'])
        expected = bytes.fromhex(row['packed'])
        if (w < 1 or h < 1 or x < 0 or y < 0 or x+w > width
                or y+h > height or len(expected) != w*h*2):
            raise ValueError('Invalid chart bounds or bytes')
        for v in range(h):
            for u in range(w):
                at = morton_address(x+u, y+v, width, height)*2
                if at in occupied:
                    raise ValueError('Overlapping chart allocations')
                occupied.add(at)
                offset = (v*w+u)*2
                if actual[at:at+2] != expected[offset:offset+2]:
                    raise ValueError(f"Atlas texel differs: map {row['map']} at ({u},{v})")
    if not occupied:
        raise ValueError('No chart texels compared')
    return len(occupied)


def capture_atlas(monitor, symbol, reference, output_dir):
    """Read generated charts from live GPU allocations in an already paused guest.

    The caller must use a settled fixture whose chart contents do not change
    between this snapshot and the PC reference endpoint.
    """
    with reference.open(newline='') as source:
        rows = list(csv.DictReader(source))
    assert rows, 'No generated charts in reference'
    table = words(monitor, symbol('stream_textures'), 1)[0]
    materials = words(monitor, symbol('stream_materials'), 1)[0]
    lightmaps = words(monitor, symbol('stream_lightmaps'), 1)[0]
    assert table and materials and lightmaps, 'No live renderer owners'
    material_count = words(monitor, materials+4, 1)[0]
    lightmap_count = words(monitor, lightmaps+4, 1)[0]
    images = []
    for image in sorted({int(row['image']) for row in rows}):
        assert 0 <= image < lightmap_count, 'Chart image outside live owner'
        pointer, fmt, _ = words(monitor, table+(material_count+1+image)*12, 3)
        assert pointer and (fmt >> 8) & 255 == 2, 'Expected swizzled A1R5G5B5'
        assert (fmt >> 16) & 15 == 1 and (fmt >> 4) & 15 == 2
        width, height = 1 << ((fmt >> 20) & 15), 1 << ((fmt >> 24) & 15)
        assert (width, height) == (512, 512), 'Unexpected generated atlas dimensions'
        size = width*height*2
        physical = pointer & 0x03ffffff
        assert physical+size <= 64*1024*1024
        output = output_dir/f'xbox-terrain-atlas-{image}.bin'
        monitor.command('human-monitor-command', {'command-line':
            f'pmemsave 0x{physical:x} {size} "{output.as_posix()}"'})
        actual = output.read_bytes()
        charts = [row for row in rows if int(row['image']) == image]
        count = compare_atlas(actual, width, height, charts)
        images.append(dict(image=image, maps=len(charts), compared_texels=count,
                           gpu_pointer=pointer, physical_address=physical, format=fmt,
                           dump_sha256=hashlib.sha256(actual).hexdigest()))
    return dict(equal=True, images=images, reference_sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                scope='All reference chart texels including borders in live GPU-visible allocations; unused atlas space excluded. No executed draw-binding or final sampling proof.')


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
