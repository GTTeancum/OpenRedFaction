"""Execute original RF.exe region admission for installed scripted Explode requests.

This checks hardness/shallow eligibility only. It does not run cutter, room,
material, duplicate-cut, or scene publication admission.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

EXE_SHA256 = 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
exe = ROOT / 'Installed_Game/RF.exe'
if hashlib.sha256(exe.read_bytes()).hexdigest() != EXE_SHA256:
    raise ValueError('RF.exe is not the verified v180 binary')
image = pefile.PE(str(exe)).get_memory_mapped_image()
cpu = Uc(UC_ARCH_X86, UC_MODE_32)
cpu.mem_map(0x400000, (len(image) + 4095) & ~4095)
cpu.mem_write(0x400000, image)
base = 0x30000000
cpu.mem_map(base, 0x100000)
stack, stop = base + 0xe0000, base + 0xf0000
word = lambda *values: struct.pack('<' + 'I' * len(values), *values)
flt = lambda *values: struct.pack('<' + 'f' * len(values), *values)
read_word = lambda address: struct.unpack('<I', cpu.mem_read(address, 4))[0]
current_regions = []


def container_call(machine, address, _size, _user):
    if address not in (0x40a490, 0x40a480):
        return
    if machine.reg_read(UC_X86_REG_ECX) != 0x6460a4:
        raise ValueError('unexpected region container')
    sp = machine.reg_read(UC_X86_REG_ESP)
    result = len(current_regions) if address == 0x40a490 else base + 0x2000 + read_word(sp + 4) * 4
    machine.reg_write(UC_X86_REG_EAX, result)
    machine.reg_write(UC_X86_REG_EIP, read_word(sp))
    machine.reg_write(UC_X86_REG_ESP, sp + (4 if address == 0x40a490 else 8))


cpu.hook_add(UC_HOOK_CODE, container_call)
cpu.mem_write(0x1754474, word(7))
events = json.loads((ROOT / 'artifacts/events.json').read_text())
census = json.loads((ROOT / 'artifacts/crater-shading-re/campaign_geomod_regions.json').read_text())
by_level = {row['level']: row for row in census['rows']}
results = []
for level in events['results']:
    if level['file'] not in by_level:
        continue
    region_row = by_level[level['file']]
    current_regions = region_row['regions']
    if len(current_regions) > 128:
        raise ValueError('region scratch layout exceeded')
    for event in level['records']:
        if event['type'].lower() != 'explode' or event['flags'][0] != 1:
            continue
        requested = event['values'][0]
        if requested < 1:
            continue  # Original master refuses before region admission.
        cpu.mem_write(base, bytes(0x10000))
        cpu.mem_write(base, flt(*event['position']))
        cpu.mem_write(base + 0x103c, flt(requested))
        cpu.mem_write(0x646004, word(region_row['default_hardness'] or 55))
        cpu.mem_write(0x647c9c, word(0))
        for index, region in enumerate(current_regions):
            address = base + 0x3000 + index * 0x50
            cpu.mem_write(base + 0x2000 + index * 4, word(address))
            cpu.mem_write(address, word(region['flags'] & 7, region['hardness']) +
                bytes([bool(region['flags'] & 32), bool(region['flags'] & 64), 0, 0]) +
                flt(region['depth'] or 0))
            cpu.mem_write(address + 0x10, flt(*region['position']))
            basis = region['basis'] or [0, 0, 1, 1, 0, 0, 0, 1, 0]
            cpu.mem_write(address + 0x1c, flt(*basis[3:6], *basis[6:9], *basis[:3]))
            cpu.mem_write(address + 0x40, flt(0, *region['size']) if region['flags'] & 4
                else flt(region['size'][0], 0, 0, 0))
        cpu.mem_write(stack, word(stop, base, base + 0x1000, 1))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
        cpu.emu_start(0x45cff0, stop, count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise ValueError('region routine did not return')
        accepted = cpu.reg_read(UC_X86_REG_EAX) & 255
        scale = struct.unpack('<f', cpu.mem_read(base + 0x103c, 4))[0]
        flags = read_word(base + 0x1038)
        results.append(dict(level=level['file'], archive=level['archive'], uid=event['uid'],
            position=event['position'], requested_radius=requested, region_allowed=bool(accepted),
            effective_scale=scale, material_flags=flags, links=event['links']))

report = dict(binary_sha256=EXE_SHA256, cases=len(results), results=results,
    scope='Original 45cff0 region selection at authored Explode centers; no cutter or scene publication')
output = ROOT / 'artifacts/crater-shading-re/scripted_geomod_admission.json'
output.write_text(json.dumps(report, indent=2) + '\n')
for row in results:
    print(row['level'], row['uid'], 'radius', row['requested_radius'],
          'region_allowed', int(row['region_allowed']), 'scale', row['effective_scale'],
          'flags', row['material_flags'])
print('cases', len(results), 'report', output)
