"""Compare C level starts with original loader and SP startup instructions.

The file gate and raw read are fixtures; generic entity creation is a stop boundary.
This does not execute the full level loader or player factory.
"""
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

exe = ROOT / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
image = pefile.PE(str(exe)).get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image) + 4095) // 4096 * 4096)
u.mem_write(0x400000, image)
STACK, PLAYER, STOP = 0x30000000, 0x30100000, 0x30200000
for address in (STACK, PLAYER, STOP):
    u.mem_map(address, 65536)


def words(address, count):
    return struct.unpack('<' + 'I' * count, u.mem_read(address, count * 4))


def put(address, *values):
    u.mem_write(address, struct.pack('<' + 'I' * len(values), *values))


raw = b''
cursor = 0
reads = []
stop_at = STOP


def run(begin, end):
    global stop_at
    stop_at = end
    u.emu_start(begin, STOP, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == end


def hook(uc, address, size, context):
    global cursor
    if address == stop_at:
        uc.emu_stop()
        return
    if address not in (0x523990, 0x52cf60):
        return
    sp = uc.reg_read(UC_X86_REG_ESP)
    if address == 0x523990:
        ret, gate = words(sp, 2)
        assert gate == 0
        uc.reg_write(UC_X86_REG_EAX, 1)  # Present binary field.
        cleanup = 4
    else:
        ret, destination, length, gate, mode = words(sp, 5)
        assert (length, gate, mode) == (12, 0, 0)
        assert cursor + length <= len(raw)
        uc.mem_write(destination, raw[cursor:cursor + length])
        reads.append(destination)
        cursor += length
        cleanup = 16
    uc.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
    uc.reg_write(UC_X86_REG_EIP, ret)


u.hook_add(UC_HOOK_CODE, hook)
inventory = json.loads((ROOT / 'artifacts/inventory.json').read_text())
levels = json.loads((ROOT / 'artifacts/levels.json').read_text())
reports = []
for level in levels:
    archive = next(f for f in inventory['files'] if f['path'] == level['archive'])
    entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
    archive_path = Path(inventory['root']) / level['archive']
    with archive_path.open('rb') as stream:
        stream.seek(entry['offset'] + level['player_start_offset'] + 8)
        raw = stream.read(48)
    assert len(raw) == 48
    cursor, reads = 0, []
    u.mem_write(0x6460fc, b'\xa5' * 48)
    put(STACK + 0x8000, STOP, 0x12345678)
    u.reg_write(UC_X86_REG_ESP, STACK + 0x8000)
    run(0x463d20, STOP)
    assert u.reg_read(UC_X86_REG_EIP) == STOP
    assert cursor == 48
    assert reads == [0x6460fc, 0x646120, 0x646108, 0x646114]
    original = bytes(u.mem_read(0x6460fc, 48))
    lines = subprocess.check_output([
        str(ROOT / 'build/pc/Release/rf_pc.exe'), str(archive_path), level['file']
    ]).decode('cp1252').splitlines()
    shared = struct.pack('<12f', *[float(v) for line in lines[1:5] for v in line.split()[1:]])
    assert original == shared, level['file']

    # Execute the actual SP copy/argument preparation, stopping at factory entry.
    u.mem_write(STACK + 0x7000, b'\xa5' * 4096)
    sp = STACK + 0x7000
    put(sp + 0x10, 0)  # Ordinary load mode, neither 9 nor 10.
    put(0x7c75d4, PLAYER)
    put(PLAYER + 0x18, 0x12345678)  # Opaque class identity, not a real class.
    u.mem_write(0x64ecb9, b'\0')
    u.mem_write(0x6fc4d9, b'\0')
    u.reg_write(UC_X86_REG_ESP, sp)
    run(0x45c798, 0x4a4130)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4a4130
    ret, player, cls, position, orientation, skin = words(u.reg_read(UC_X86_REG_ESP), 6)
    assert (ret, player, cls, skin) == (0x45c80c, PLAYER, 0x12345678, 0xffffffff)
    assert bytes(u.mem_read(position, 12)) + bytes(u.mem_read(orientation, 36)) == shared
    put(PLAYER + 0xc, PLAYER + 0x2000)  # Player name string storage.
    put(PLAYER + 0x10, 0xa5a5a5ad)
    put(PLAYER + 0xf5c, 7)
    put(0x6c9c60, 3)
    u.mem_write(0x7c75c8, b'\0')
    u.mem_write(0x7c73b4, b'\0')
    run(0x4a4130, 0x422360)
    assert u.reg_read(UC_X86_REG_EIP) == 0x422360
    args = words(u.reg_read(UC_X86_REG_ESP), 8)
    assert args == (0x4a41d8, cls, PLAYER + 0x2000, 0xffffffff, position, orientation, 1, 0)
    assert words(PLAYER + 0x10, 1) == (0xa5a5a5a5,)
    assert words(PLAYER + 0xf5c, 1) == (0,)
    assert bytes(u.mem_read(position, 12)) + bytes(u.mem_read(orientation, 36)) == shared
    reports.append(dict(file=level['file'], payload_sha256=hashlib.sha256(raw).hexdigest(),
                        transform_words=list(struct.unpack('<12I', shared)), result='PASS'))

prefix_cases = []
compiled_inputs, compiled_expected = bytearray(), bytearray()
for local in (False, True):
    for skin in (-1, 0, 2, 3):
        for override in (0, 1, 2):
            sp = STACK + 0x7000
            position, orientation = STACK + 0x9000, STACK + 0x9010
            u.mem_write(position, shared[:12])
            u.mem_write(orientation, shared[12:])
            alternate = struct.pack('<3f', 12.5, -7.25, 100.0)
            compiled_inputs.extend(struct.pack('<IiiII',0xa5a5a5ad,7,int(local),3,override)
                                   + alternate + struct.pack('<i',skin) + shared[:12])
            u.mem_write(0x7c7628, alternate)
            u.mem_write(0x7c75c8, bytes([override]))
            put(0x7c75d4, PLAYER if local else PLAYER + 0x4000)
            put(PLAYER + 0x10, 0xa5a5a5ad)
            put(PLAYER + 0xf5c, 7)
            put(sp, STOP, PLAYER, 0x12345678, position, orientation, skin & 0xffffffff)
            u.reg_write(UC_X86_REG_ESP, sp)
            run(0x4a4130, 0x422360)
            assert u.reg_read(UC_X86_REG_EIP) == 0x422360
            actual = words(u.reg_read(UC_X86_REG_ESP), 8)
            normalized = skin if 0 <= skin < 3 else 0
            assert actual == (0x4a41d8, 0x12345678, PLAYER + 0x2000, 0xffffffff,
                              position, orientation, 1, normalized)
            assert words(PLAYER + 0x10, 1) == ((0xa5a5a5a5 if local else 0xa5a5a5ad),)
            assert words(PLAYER + 0xf5c, 1) == ((0 if local and skin not in (0, 2) else 7),)
            assert bytes(u.mem_read(position, 12)) == (alternate if override == 1 else shared[:12])
            assert bytes(u.mem_read(orientation, 36)) == shared[12:]
            assert bytes(u.mem_read(0x7c75c8, 1)) == bytes([0 if override == 1 else override])
            compiled_expected.extend(bytes(u.mem_read(PLAYER+0x10,4)) + bytes(u.mem_read(PLAYER+0xf5c,4))
                + struct.pack('<iiI',int(local),3,u.mem_read(0x7c75c8,1)[0]) + alternate
                + struct.pack('<I',actual[7]) + bytes(u.mem_read(position,12)))
            prefix_cases.append(dict(local=local, skin=skin, position_override_flag=override, result='PASS'))

probe = ROOT / 'build/pc/Release/rf_entity_probe.exe'
compiled = subprocess.check_output([str(probe),'--player-spawn'],input=compiled_inputs)
assert compiled == compiled_expected, 'Compiled PC spawn preparation differs from original'
nxdk_verified = False
if '--nxdk' in sys.argv:
    pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
    data = pe.get_memory_mapped_image()
    nx = Uc(UC_ARCH_X86,UC_MODE_32)
    nx.mem_map(pe.OPTIONAL_HEADER.ImageBase,(len(data)+4095)//4096*4096)
    nx.mem_write(pe.OPTIONAL_HEADER.ImageBase,data)
    nx.mem_map(STACK,65536)
    mapping = (ROOT / 'build/xbox/main.map').read_text()
    address = int(re.search(r'_rf_player_spawn_prepare\s+([0-9a-fA-F]+)',mapping)[1],16)
    for offset in range(0,len(compiled_inputs),48):
        wire = bytes(compiled_inputs[offset:offset+48])
        nx.mem_write(STACK,wire)
        nx.mem_write(STACK+0x8000,struct.pack('<6I',STACK+0xf000,STACK,
            struct.unpack_from('<I',wire,8)[0],3,STACK+16,STACK+32))
        nx.reg_write(UC_X86_REG_ESP,STACK+0x8000)
        nx.emu_start(address,STACK+0xf000,count=10000)
        assert nx.reg_read(UC_X86_REG_EIP)==STACK+0xf000
        assert nx.reg_read(UC_X86_REG_EAX)==0
        assert bytes(nx.mem_read(STACK,48))==compiled_expected[offset:offset+48]
    nxdk_verified = True

report = dict(result='PASS', original_sha256=digest,
              shared_exe_sha256=hashlib.sha256((ROOT / 'build/pc/Release/rf_pc.exe').read_bytes()).hexdigest(),
              scope='Original 463d20 loader with file gate/read fixtures; 45c798 SP startup and 4a4130 prefix to 422360 entry; no generic entity creation or orientation override',
              levels=reports, factory_prefix_cases=prefix_cases,
              compiled_pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
              compiled_nxdk_verified=nxdk_verified,
              compiled_nxdk_sha256=hashlib.sha256((ROOT/'build/xbox/main.exe').read_bytes()).hexdigest() if nxdk_verified else None)
(ROOT / 'artifacts/player-start-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'PASS: {len(reports)} original player starts; {len(prefix_cases)} factory prefix cases match compiled PC; NXDK={nxdk_verified}')
