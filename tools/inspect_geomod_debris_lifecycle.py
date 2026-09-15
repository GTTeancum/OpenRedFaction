"""Execute original48fd70 age/fade branches with pause, unlink and draw hooks.

The original x87 arithmetic, float-to-integer conversion and alpha clamp run
unmodified. This verifies the draw-time lifetime policy, not chunk physics.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW
from extract_geomod_template import SHA


def main():
    exe = ROOT / 'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
    pe = pefile.PE(str(exe)); image = pe.get_memory_mapped_image()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, (len(image)+4095)&~4095); cpu.mem_write(0x400000, image)
    base = 0x30000000; stack = base+0xe000; stop = base+0xf000
    cpu.mem_map(base, 0x10000)
    pack = lambda *v: struct.pack('<'+'I'*len(v), *v)
    floats = lambda *v: struct.pack('<'+'f'*len(v), *v)
    f32 = lambda v: struct.unpack('<f', floats(v))[0]
    read = lambda p: struct.unpack('<I', cpu.mem_read(p, 4))[0]
    events = []; paused = 0

    def hook(u, address, size, context):
        if address not in (0x436320, 0x48f3d0, 0x517080): return
        sp = u.reg_read(UC_X86_REG_ESP)
        if address == 0x436320: u.reg_write(UC_X86_REG_EAX, paused)
        elif address == 0x48f3d0:
            assert read(sp+4) == base
            events.append(('remove', None))
        else:
            assert read(sp+4) == base
            events.append(('draw', read(sp+8)))
        u.reg_write(UC_X86_REG_EIP, read(sp)); u.reg_write(UC_X86_REG_ESP, sp+4)

    cpu.hook_add(UC_HOOK_CODE, hook)
    rows = []
    for lifetime in (1., 1.375, 2.5, 3.999):
        lifetime = f32(lifetime)
        threshold = struct.unpack('<I', floats(lifetime+1))[0]
        ages = [0., lifetime-.1, lifetime, lifetime+.1, lifetime+.5,
                *[struct.unpack('<f', pack(threshold+n))[0] for n in (-1, 0, 1)]]
        for age in ages:
            age = f32(age)
            for dt in (0., 1/60, .25, 2.):
                dt = f32(dt)
                for paused in (0, 1):
                    events.clear(); cpu.mem_write(base, bytes(128))
                    cpu.mem_write(base+0x70, floats(age, lifetime))
                    cpu.mem_write(0x5a4014, floats(dt)); cpu.mem_write(stack, pack(stop, base))
                    cpu.reg_write(UC_X86_REG_ESP, stack); cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
                    cpu.emu_start(0x48fd70, stop, count=10000)
                    assert cpu.reg_read(UC_X86_REG_EIP) == stop
                    after = struct.unpack('<f', cpu.mem_read(base+0x70, 4))[0]
                    removed = age > lifetime+1.
                    expected_age = age if removed or paused else f32(age+dt)
                    alpha = 255 if expected_age <= lifetime else max(0, min(255, int((1.-(expected_age-lifetime))*255)))
                    assert after == expected_age, (age, dt, after, expected_age)
                    expected_events = [('remove', None)] if removed else [('draw', alpha)]
                    assert events == expected_events, (age, dt, events, alpha)
                    shared = subprocess.check_output([str(ROOT/'build/pc/Release/rf_geomod_basis_probe.exe'),
                        '--debris-age', str(age), str(lifetime), str(dt), str(paused)], text=True).split()
                    assert floats(float(shared[0])) == floats(after)
                    assert list(map(int, shared[1:])) == [int(removed), 0 if removed else alpha]
                    rows.append(dict(lifetime=lifetime, age=age, dt=dt, paused=paused,
                                     after=after, removed=removed, alpha=None if removed else alpha))
    output = dict(exe_sha256=SHA, entry='0048fd70', scope=__doc__, cases=rows)
    (ROOT/'artifacts/geomod-debris-lifecycle-original.json').write_text(json.dumps(output, indent=2)+'\n')
    print('PASS:', len(rows), 'original/shared debris age/fade cases; removal is checked before advancing age')


if __name__ == '__main__': main()
