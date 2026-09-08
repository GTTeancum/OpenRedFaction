"""Execute original solid-mode initializer in isolated x86 emulation; no game process."""
import hashlib
import json
import sys
from pathlib import Path
import pefile
import capstone

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP

binary = root/'Installed_Game/RF.exe'
fingerprint = hashlib.sha256(binary.read_bytes()).hexdigest()
assert fingerprint == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(binary))
image = pe.get_memory_mapped_image()
base = pe.OPTIONAL_HEADER.ImageBase
emulator = Uc(UC_ARCH_X86, UC_MODE_32)
emulator.mem_map(base, (len(image)+4095)//4096*4096)
emulator.mem_write(base, image)
stack, stop = 0x30000000, 0x31000000
emulator.mem_map(stack, 4096)
emulator.mem_map(stop, 4096)
emulator.mem_write(stack+4080, stop.to_bytes(4, 'little'))
emulator.reg_write(UC_X86_REG_ESP, stack+4080)
emulator.emu_start(0x515730, stop, count=1000)
mode = int.from_bytes(emulator.mem_read(0x1808328, 4), 'little')
fields = [(mode >> shift) & 31 for shift in range(0, 30, 5)]
assert fields == [4, 1, 0, 0, 4, 0], fields
disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
instructions = {}
for start, length in [(0x515730, 23), (0x411e00, 56)]:
    instructions[hex(start)] = [f'{i.address:08x}: {i.mnemonic} {i.op_str}'
                                for i in disassembler.disasm(pe.get_data(start-base, length), start)]
report = dict(sha256=fingerprint, initializer='0x515730', constructor='0x411e00',
              destination='0x1808328', mode=hex(mode), fields=fields, instructions=instructions,
              limitation='Initializer only; runtime flag 0x1cfcc1d can remap texture-source 4 to 5 in 0x54f160.')
(root/'artifacts/render-state.json').write_text(json.dumps(report, indent=2))
print(json.dumps({k:v for k,v in report.items() if k != 'instructions'}, indent=2))
