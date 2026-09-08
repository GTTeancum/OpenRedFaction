"""Find candidate eye-field operands; offsets alone do not establish semantics."""
import hashlib
import json
from pathlib import Path
import capstone
import pefile

root = Path(__file__).resolve().parents[1]
binary = root/'Installed_Game/RF.exe'
sha = hashlib.sha256(binary.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(binary))
section = next(s for s in pe.sections if s.Name.startswith(b'.text'))
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
decoder.detail = True
decoder.skipdata = True
matches = []
for instruction in decoder.disasm(section.get_data(), pe.OPTIONAL_HEADER.ImageBase+section.VirtualAddress):
    if instruction.id and any(op.type == capstone.x86.X86_OP_MEM and op.mem.disp in (0x7d4, 0x7d8, 0x7dc) for op in instruction.operands):
        matches.append(dict(address=hex(instruction.address), mnemonic=instruction.mnemonic, operands=instruction.op_str))
report = dict(sha256=sha, candidates=matches,
              limitation='Linear instruction scan; operands may belong to unrelated structures. Classify using callers and register data flow.')
(root/'artifacts/entity-eye-candidates.json').write_text(json.dumps(report, indent=2))
print(f'{len(matches)} candidate field references; manual data-flow classification required')
