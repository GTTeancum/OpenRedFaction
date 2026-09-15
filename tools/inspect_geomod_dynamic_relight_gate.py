"""Execute the original dynamic-light query gate at 4f2c79.

Stops before the light query, lighting setup or upload-only path. This verifies
branch selection and query arguments, not dirty-flag scheduling or final pixels.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'local/python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP
from extract_geomod_template import SHA


def main():
    exe = ROOT/'Installed_Game/RF.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest() == SHA
    image = pefile.PE(str(exe)).get_memory_mapped_image()
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, (len(image)+4095) & ~4095)
    cpu.mem_write(0x400000, image)
    base = 0x30000000
    stack = base+0xe000
    cpu.mem_map(base, 65536)
    reached = []

    def stop(machine, address, size, data):
        if address in (0x4d9c00, 0x4d9fd0, 0x4f2f0a):
            reached.append(address)
            machine.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, stop)
    rows = []
    for dirty in range(256):
        for inhibit in (0, 1):
            for query_bypass in (0, 1):
                reached.clear()
                cpu.mem_write(base, bytes(128))
                cpu.mem_write(base+8, bytes((dirty, 0, inhibit)))
                cpu.mem_write(base+0x68, struct.pack('<I', 0xffffffff))
                cpu.mem_write(0xc96890, bytes((query_bypass,)))
                cpu.mem_write(stack-64, bytes(256))
                cpu.reg_write(UC_X86_REG_ESI, base)
                cpu.reg_write(UC_X86_REG_ESP, stack)
                cpu.emu_start(0x4f2c79, 0x4f3000, count=100)
                expected = (0x4f2f0a if not dirty & 1 else
                            0x4d9fd0 if query_bypass else 0x4d9c00)
                assert reached == [expected], (dirty, inhibit, query_bypass, reached)
                if expected == 0x4d9c00:
                    esp = cpu.reg_read(UC_X86_REG_ESP)
                    arguments = struct.unpack('<5I', cpu.mem_read(esp+4, 20))
                    assert arguments == (0, base+0x34, base+0x40, 1, 0), arguments
                rows.append(dict(dirty=dirty, inhibit=inhibit, query_bypass=query_bypass,
                                 dynamic_branch=bool(dirty & 1), queries_lights=expected == 0x4d9c00))
    output = ROOT/'artifacts/geomod-dynamic-relight-gate-original.json'
    output.write_text(json.dumps(dict(scope=__doc__, exe_sha256=SHA, cases=rows), indent=2)+'\n')
    print('PASS: 1024 original dynamic-gate cases and query arguments; dirty8 bypasses this branch')


if __name__ == '__main__':
    main()
