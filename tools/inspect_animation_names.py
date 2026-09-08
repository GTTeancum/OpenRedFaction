"""Read animation-state/action names from the fingerprinted PC initializers.

No original constructors are replaced or executed: each immediate string push,
destination ECX and constructor call is checked in the original instruction
stream. Output is local reverse-engineering evidence, not runtime game data.
"""
import hashlib
import json
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

root = Path(__file__).resolve().parents[1]
exe = root / 'Installed_Game/RF.exe'
digest = hashlib.sha256(exe.read_bytes()).hexdigest()
assert digest == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(exe))
image = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
decoder = Cs(CS_ARCH_X86, CS_MODE_32)
report = {'executable_sha256': digest}
for kind, address, table, count in [('states', 0x418030, 0x62f208, 23),
                                   ('actions', 0x4181d0, 0x5caee0, 45)]:
    # Each initialization is push imm32; mov ecx,imm32; call rel32 (15 bytes).
    code = image[address-base:address-base+count*15+1]
    instructions = list(decoder.disasm(code, address))
    assert len(instructions) == count*3+1 and instructions[-1].mnemonic == 'ret'
    names = []
    for i in range(count):
        push, move, call = instructions[i*3:i*3+3]
        assert push.mnemonic == 'push' and push.size == 5
        assert move.mnemonic == 'mov' and move.op_str == f'ecx, 0x{table+i*8:x}'
        assert call.mnemonic == 'call' and call.op_str == '0x4ff3d0'
        pointer = int(push.op_str, 16)
        assert base <= pointer < base+len(image)
        name = image[pointer-base:].split(b'\0', 1)[0].decode('ascii')
        assert 0 < len(name) < 64
        names.append({'index': i, 'name': name, 'string_address': hex(pointer)})
    report[kind] = {'initializer': hex(address), 'table': hex(table), 'names': names}
assert [x['name'] for x in report['actions']['names'][17:21]] == [
    'sidestep_left', 'sidestep_right', 'roll_left', 'roll_right']
output = root / 'artifacts/animation-names.json'
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps(report, indent=2))
print('PASS: 23 state names and 45 action names checked against original initializer instructions')
print(output)
