"""Original461ff0 ambient loader: supplied stream/string and constructor boundaries."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import pefile
from verify_ambient_records import inspect

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

exe = root / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe)); image = pe.get_memory_mapped_image()
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, (len(image)+4095)//4096*4096); u.mem_write(0x400000, image)
base = 0x30000000; stack = base+0xe000; stop = base+0xf000
u.mem_map(0, 4096); u.mem_map(base, 65536)
word = lambda *values: struct.pack('<'+'I'*len(values), *values)
read = lambda address: struct.unpack('<I', u.mem_read(address, 4))[0]
float_return = base+0xc000; float_value = base+0xc100; name_buffer = base+0x1000
u.mem_write(float_return, b'\xd9\x05'+word(float_value)+b'\xc2\x08\x00')
data = b''; cursor = 0; calls = []

def take(n):
    global cursor
    assert cursor+n <= len(data)
    result = data[cursor:cursor+n]; cursor += n
    return result

def hook(machine, address, size, context):
    if address not in (0x4ff3b0, 0x4ff470, 0x4ff480, 0x5239c0,
                       0x52c910, 0x52ca00, 0x52c780, 0x52cc10, 0x52c9b0, 0x45aca0):
        return
    sp = machine.reg_read(UC_X86_REG_ESP); pop = 0; result = 0
    if address == 0x4ff480:
        result = name_buffer
    elif address == 0x5239c0:
        result = 180
    elif address == 0x52c910:
        result, = struct.unpack('<I', take(4)); pop = 8
    elif address == 0x52ca00:
        machine.mem_write(read(sp+4), take(12)); pop = 12
    elif address == 0x52c780:
        result = int(take(1)[0] != 0); pop = 8
    elif address == 0x52cc10:
        length, = struct.unpack('<H', take(2))
        assert length < 256
        machine.mem_write(name_buffer, take(length)+b'\0'); pop = 12
    elif address == 0x52c9b0:
        machine.mem_write(float_value, take(4))
        machine.reg_write(UC_X86_REG_EIP, float_return); return
    elif address == 0x45aca0:
        args = list(struct.unpack('<7I', machine.mem_read(sp+4, 28)))
        name = bytes(machine.mem_read(args[1], 256)).split(b'\0', 1)[0]
        calls.append((args[0], name, bytes(machine.mem_read(args[2], 12)), word(*args[3:])))
    machine.reg_write(UC_X86_REG_EAX, result)
    machine.reg_write(UC_X86_REG_EIP, read(sp))
    machine.reg_write(UC_X86_REG_ESP, sp+4+pop)

u.hook_add(UC_HOOK_CODE, hook)
inventory = json.loads((root/'artifacts/inventory.json').read_text())
levels = json.loads((root/'artifacts/levels.json').read_text())
sections = records = 0
for level in levels:
    section = next((s for s in level['sections'] if s['type'] == '0x500'), None)
    if not section:
        continue
    archive = next(a for a in inventory['files'] if a['path'] == level['archive'])
    entry = next(e for e in archive['vpp']['entries'] if e['name'] == level['file'])
    with (root/'Installed_Game'/level['archive']).open('rb') as stream:
        stream.seek(entry['offset']+section['offset']+8); data = stream.read(section['size'])
    rows, _ = inspect(data)
    expected = [(r['uid'], r['name'].encode('cp1252'), struct.pack('<3f', *r['position']),
                 struct.pack('<3fI', r['near_distance'], r['volume'], r['rolloff'], r['flags'])) for r in rows]
    for suppressed in (0, 1):
        cursor = 0; calls = []
        u.mem_write(0x64ecbb, bytes([suppressed])); u.mem_write(stack, word(stop, base+0x4000))
        u.reg_write(UC_X86_REG_ESP, stack); u.reg_write(UC_X86_REG_FPCW, 0x27f)
        u.emu_start(0x461ff0, stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == stop and cursor == len(data), level['file']
        assert calls == ([] if suppressed else expected), level['file']
    sections += 1; records += len(rows)
report = dict(result='PASS', sections=sections, records=records, original_sha256=digest,
              scope='Original461ff0 full iteration and constructor arguments, with version180 '
                    'stream reads, string operations and45aca0 constructor supplied. '
                    'Normal and64ecbb-suppressed creation consume identical full sections. '
                    'No original filesystem parser, sound registration or playback proof.')
(root/'artifacts/ambient-loader.json').write_text(json.dumps(report, indent=2))
print(report)
