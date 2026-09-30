"""Compare original SP vital callbacks with NXDK code, without launching PC gameplay."""
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import pefile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_FPCW
BASE = 0x30000000
STACK, STOP = BASE + 0xe000, BASE + 0xf000
pack = lambda *values: struct.pack('<' + 'I' * len(values), *values)
word = lambda cpu, address: struct.unpack('<I', bytes(cpu.mem_read(address, 4)))[0]


def load(path):
    pe = pefile.PE(str(path)); blob = pe.get_memory_mapped_image(); origin = pe.OPTIONAL_HEADER.ImageBase
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(origin, (len(blob)+4095)//4096*4096); cpu.mem_write(origin, blob)
    cpu.mem_map(BASE, 0x10000)
    return cpu


def main():
    original_path = ROOT / 'Installed_Game/RF.exe'
    digest = hashlib.sha256(original_path.read_bytes()).hexdigest()
    assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    original, xbox = load(original_path), load(ROOT / 'build/xbox/main.exe')
    mapping = (ROOT / 'build/xbox/main.map').read_text()
    entry = int(re.search(r'_rf_entity_vital_pickup_sp\s+([0-9a-fA-F]+)', mapping)[1], 16)

    def hook(cpu, address, size, context):
        stack = cpu.reg_read(UC_X86_REG_ESP)
        value = {0x426fc0: BASE, 0x459a20: BASE+0x1000, 0x4895d0: 0}[address]
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_EIP, word(cpu, stack))
        cpu.reg_write(UC_X86_REG_ESP, stack+4)

    for address in (0x426fc0, 0x459a20, 0x4895d0):
        original.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    cases = []
    for armor in (False, True):
        for current, maximum, quantity, difficulty in [(10, 200, 25, d) for d in range(4)] + [(74.5, 80, 25, 1), (0, 0, 25, 1), (150, 150, 25, 1)]:
            field = 0x38 if armor else 0x34; cap = 0x48 if armor else 0x44
            original.mem_write(BASE+0x294, pack(BASE+0x2000))
            original.mem_write(BASE+field, struct.pack('<f', current))
            original.mem_write(BASE+0x2000+cap, struct.pack('<f', maximum))
            original.mem_write(0x64ecb9, b'\0'); original.mem_write(0x6fc4d8, b'\0')
            original.mem_write(0x593e54, pack(difficulty))
            original.mem_write(STACK, pack(STOP, 1, 2, quantity))
            original.reg_write(UC_X86_REG_ESP, STACK); original.reg_write(UC_X86_REG_FPCW, 0x37f)
            original.emu_start(0x45a1f0 if armor else 0x45a2e0, STOP, count=10000)
            assert original.reg_read(UC_X86_REG_EIP) == STOP
            expected = bytes(original.mem_read(BASE+field, 4))
            xbox.mem_write(BASE, struct.pack('<2f', current, -123))
            maximum_bits = struct.unpack('<I', struct.pack('<f', maximum))[0]
            xbox.mem_write(STACK, pack(STOP, BASE, maximum_bits, quantity, difficulty, BASE+4))
            xbox.reg_write(UC_X86_REG_ESP, STACK)
            xbox.emu_start(entry, STOP, count=10000)
            assert xbox.reg_read(UC_X86_REG_EIP) == STOP and xbox.reg_read(UC_X86_REG_EAX) == 0
            assert bytes(xbox.mem_read(BASE, 4)) == expected, (armor, current, maximum, difficulty, struct.unpack("<f", expected)[0], struct.unpack("<f", xbox.mem_read(BASE, 4))[0])
            restored = struct.unpack('<f', xbox.mem_read(BASE+4, 4))[0]
            assert restored == struct.unpack('<f', expected)[0]-current
            cases.append(dict(armor=armor, current=current, maximum=maximum, quantity=quantity, difficulty=difficulty, restored=restored))
    report = {'result': 'PASS', 'original_sha256': digest, 'cases': cases,
              'scope': '14 original callback/NXDK math cases; lookup boundaries stubbed, SP difficulty/float math executed'}
    (ROOT / 'artifacts/xbox-vital-pickup.json').write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: 14 original/NXDK vital pickup cases')


if __name__ == '__main__':
    main()
