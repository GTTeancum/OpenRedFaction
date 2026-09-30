"""Read installed item sound metadata with the compiled NXDK parser; no PC game."""
import json
from pathlib import Path
import re
import struct
import sys
import pefile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
from build_fragment_platform_fixture import read_entry


def main():
    pe = pefile.PE(str(ROOT / 'build/xbox/main.exe'))
    blob = pe.get_memory_mapped_image()
    origin = pe.OPTIONAL_HEADER.ImageBase
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(origin, (len(blob) + 4095) // 4096 * 4096)
    cpu.mem_write(origin, blob)
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    text, name, output, stack, stop = base, base + 0x20000, base + 0x21000, base + 0x30000, base + 0x31000
    mapping = (ROOT / 'build/xbox/main.map').read_text()
    entry = int(re.search(r'_rf_item_definition_read\s+([0-9a-fA-F]+)', mapping)[1], 16)
    table = read_entry(ROOT / 'Installed_Game/tables.vpp', 'items.tbl')
    assert len(table) < 0x20000

    def parse(data, label):
        cpu.mem_write(text, data)
        cpu.mem_write(name, label.encode() + b'\0')
        cpu.mem_write(output, b'\xa5' * 472)
        cpu.mem_write(stack, struct.pack('<6I', stop, text, len(data), name, output, 0))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(entry, stop, count=3000000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        return cpu.reg_read(UC_X86_REG_EAX), bytes(cpu.mem_read(output, 472))

    cases = []
    for label in ['Handgun', '12mm_ammo', 'Miner Envirosuit']:
        status, result = parse(table, label)
        assert status == 0, (label, status)
        sound = result[400:464].split(b'\0', 1)[0]
        if label == 'Miner Envirosuit':
            assert sound == b'envsuit_pickup.wav'
            assert result[464:472] == struct.pack('<2f', 5, .9)
        else:
            assert not sound and result[64:128].split(b'\0', 1)[0]
        cases.append(label + ': authored override/default selection metadata')
    malformed = table.replace(b'"envsuit_pickup.wav"', b'"envsuit_pickup.wav" "bad"', 1)
    status, result = parse(malformed, 'Miner Envirosuit')
    assert status != 0 and result == b'\xa5' * 472
    cases.append('Malformed override rejects without publishing output')
    report = {'result': 'PASS', 'scope': 'Compiled NXDK item parser', 'cases': cases}
    (ROOT / 'artifacts/xbox-item-sound.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
