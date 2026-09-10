"""Compare shared player-contact response with original instruction fixtures."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

import pefile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'local/python'))
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW, UC_X86_REG_EAX

subprocess.run([sys.executable, str(root/'tools/inspect_player_contact.py')], cwd=root, check=True, stdout=subprocess.DEVNULL)
reference = json.loads((root/'artifacts/player-contact-reference.json').read_text())
assert reference['result'] == 'PASS'
cases = reference['records']
pack = lambda values: struct.pack('<'+'I'*len(values), *values)
commands = b''.join(pack(c['input_words']+[c['mode'], int(c['mode'] in (3, 8))]) for c in cases)
probe = root/'build/pc/Release/rf_physics_probe.exe'
actual = subprocess.check_output([str(probe), '--player-contact'], input=commands)
assert actual == b''.join(pack(c['output_words']) for c in cases), 'PC player contact mismatch'
binary = root/'build/xbox/main.exe'
pe = pefile.PE(str(binary))
image = pe.get_memory_mapped_image()
origin = pe.OPTIONAL_HEADER.ImageBase
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(origin, (len(image)+4095)//4096*4096)
u.mem_write(origin, image)
base, stack, stop = 0x30000000, 0x3000e000, 0x3000f000
u.mem_map(base, 65536)
entry = int(re.search(r'_rf_physics_player_contact\s+([0-9a-fA-F]+)', (root/'build/xbox/main.map').read_text())[1], 16)
failures = []
for index, case in enumerate(cases):
    command = pack(case['input_words'])
    state = bytearray([0xa5]*308)
    state[112:148] = command[72:108]
    state[184:208] = command[:24]
    state[272:276] = pack([0x80])
    u.mem_write(base, bytes(state))
    u.mem_write(base+0x2000, command[24:72])
    u.mem_write(base+0x3000, bytes([0xa5]*4))
    u.mem_write(stack, pack([stop, base, base+0x2000, base+0x200c, base+0x2018, base+0x2024,
                             case['mode'], int(case['mode'] in (3, 8)), base+0x3000]))
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_FPCW, 0x27f)
    u.emu_start(entry, stop, count=100000)
    assert u.reg_read(UC_X86_REG_EIP) == stop and u.reg_read(UC_X86_REG_EAX) == 0
    assert u.reg_read(UC_X86_REG_FPCW) == 0x27f
    state[184:196] = pack(case['output_words'][:3])
    if bytes(u.mem_read(base, 308)) != state or bytes(u.mem_read(base+0x3000, 4)) != pack(case['output_words'][6:]):
        failures.append(index)
guards = 0
valid_state = bytes(state)
valid_parameters = bytes(u.mem_read(base+0x2000, 48))
for location in [('flag', 272), ('predicate', 0)] + [('state', o) for o in list(range(112,148,4))+list(range(184,196,4))] + [('parameter', o) for o in range(0,48,4)]:
    for bad in (0x7fc00000, 0x7f800000):
        guarded = bytearray(valid_state)
        parameters = bytearray(valid_parameters)
        predicate = 0
        kind, offset = location
        if kind == 'state': guarded[offset:offset+4] = pack([bad])
        elif kind == 'parameter': parameters[offset:offset+4] = pack([bad])
        elif kind == 'flag': guarded[272:276] = pack([0])
        else: predicate = 2
        u.mem_write(base, bytes(guarded))
        u.mem_write(base+0x2000, bytes(parameters))
        u.mem_write(base+0x3000, pack([0xa5a5a5a5]))
        u.mem_write(stack, pack([stop,base,base+0x2000,base+0x200c,base+0x2018,base+0x2024,1,predicate,base+0x3000]))
        u.reg_write(UC_X86_REG_ESP, stack)
        u.reg_write(UC_X86_REG_FPCW, 0x27f)
        u.emu_start(entry, stop, count=100000)
        assert u.reg_read(UC_X86_REG_EIP) == stop and u.reg_read(UC_X86_REG_EAX) == 0xfffffffc
        assert bytes(u.mem_read(base,308)) == guarded and bytes(u.mem_read(base+0x3000,4)) == pack([0xa5a5a5a5])
        assert u.reg_read(UC_X86_REG_FPCW) == 0x27f
        guards += 1
report = dict(result='FAIL' if failures else 'PASS', pc_cases=len(cases), nxdk_cases=len(cases), nxdk_guard_cases=guards,
              failures=failures, original_sha256=reference['original_sha256'],
              pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(), nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
              scope=reference['scope'].replace('No shared C comparison or full player physics claim.', 'Full player physics is outside this check.')+' Shared PC/NXDK response comparison; NXDK checks complete body preservation outside velocity and incoming FPCW 027f restoration.')
(root/'artifacts/player-contact-verification.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
assert not failures
