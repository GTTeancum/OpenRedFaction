"""Decode retained NV2A pushbuffer submissions without changing the guest."""
import hashlib
import struct
from xemu_guest_snapshot import words


def decode_draws(data):
    if len(data) % 4:
        raise ValueError('Unaligned pushbuffer')
    stream = struct.unpack('<'+'I'*(len(data)//4),data)
    at, state, draws = 0, {}, []
    while at < len(stream):
        header = stream[at]
        at += 1
        # pb_reset starts this range at the ring head; jumps/calls must not be
        # silently interpreted as methods or followed into stale ring contents.
        if header & 0xa0030003:
            raise ValueError(f'Unsupported command header {header:08x}')
        count = (header >> 18) & 0x7ff
        method, subchannel = header & 0x1ffc, (header >> 13) & 7
        if at+count > len(stream):
            raise ValueError('Truncated method packet')
        for i in range(count):
            register = method if header & 0x40000000 else method+i*4
            value = stream[at+i]
            if subchannel == 0:
                state[register] = value
                if register == 0x1810:  # NV097_DRAW_ARRAYS
                    draws.append(dict(start=value & 0xffffff,count=(value >> 24)+1,
                                      state=dict(state)))
        at += count
    return draws


def capture(monitor,symbol,material_reference,atlas_report,output_dir):
    enabled,head,size,frames = words(monitor,symbol('rf_xbox_draw_audit'),4)
    assert enabled and head and frames and 0 < size <= 256*1024

    def dump(pointer,size,name):
        physical = pointer & 0x03ffffff
        assert physical+size <= 64*1024*1024
        path = output_dir/name
        monitor.command('human-monitor-command',{'command-line':
            f'pmemsave 0x{physical:x} {size} "{path.as_posix()}"'})
        data = path.read_bytes()
        assert len(data) == size
        return data

    commands = dump(head,size,'xbox-draw-commands.bin')
    draws = decode_draws(commands)
    gpu = words(monitor,symbol('stream_gpu'),1)[0]
    capacity = words(monitor,symbol('stream_capacity'),1)[0]
    assert gpu and 0 < capacity <= 8*1024*1024
    selected = [d for d in draws if d['state'].get(0x1720) == gpu & 0x03ffffff]
    assert selected, 'No stream draws in retained command range'
    size = max(d['start']+d['count'] for d in selected)*56
    assert 0 < size <= capacity
    vertices = dump(gpu,size,'xbox-draw-vertices.bin')
    material = struct.unpack_from('<I',material_reference.read_bytes(),4)[0]
    table = words(monitor,symbol('stream_textures'),1)[0]
    base_pointer,base_format,_ = words(monitor,table+material*12,3)
    images = {item['image']:item for item in atlas_report['images']}
    caps = []
    for draw in selected:
        records = [struct.unpack_from('<9fI3fI',vertices,(draw['start']+i)*56)
                   for i in range(draw['count'])]
        if not any(v[9] == material and v[13] in images for v in records):
            continue
        assert all(v[9] == material and v[13] == records[0][13] for v in records)
        state, image = draw['state'], images[records[0][13]]
        expected = {0x1b00:base_pointer & 0x03ffffff,0x1b04:base_format,
                    0x1b40:image['physical_address'],0x1b44:image['format'],
                    0x1b08:0x00010101,0x1b48:0x00030303,
                    0x1b14:0x02020000,0x1b54:0x02020000,0x17fc:5}
        for register,value in expected.items():
            assert state.get(register) == value, ('cap draw state',hex(register),state.get(register),value)
        assert state.get(0x1b0c,0) & (1 << 30) and state.get(0x1b4c,0) & (1 << 30)
        assert all(v[3:6] == (1.,1.,1.) and v[8] == v[12] and v[8] > 0 for v in records)
        caps.append(dict(start=draw['start'],vertices=draw['count'],material=material,
                         image=records[0][13],state={hex(k):state[k] for k in expected}))
    assert caps, 'No generated cap draws found'
    return dict(result='PASS',cap_draws=caps,snapshot_frames=frames,command_bytes=len(commands),vertex_bytes=len(vertices),
                command_sha256=hashlib.sha256(commands).hexdigest(),
                scope='Retained submitted NV2A commands and their actual stream vertices; texture offsets/formats, wrap/clamp, filtering, enable and white color verified. Not GPU execution tracing or per-pixel native sampling proof.')
