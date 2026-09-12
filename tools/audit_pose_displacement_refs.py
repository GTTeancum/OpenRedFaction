"""Locate and classify direct12c0 instance/definition displacement references."""
import hashlib,json
from pathlib import Path
import pefile,capstone
from capstone.x86 import X86_OP_MEM
root=Path(__file__).resolve().parents[1];exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));d=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);d.detail=True;d.skipdata=True
classification={0x51ae28:'Instance default vector constructor boundary in51ae00; not evidence of a nonzero producer.',0x51aebd:'Instance default vector constructor boundary in51ae90; later bulk-zero initialization.',0x51af8f:'Instance explicit zero-vector assignment in51ae90.',0x51b45b:'Shared model-definition attachment transform, not instance pending displacement.',0x51b8e9:'Instance root evaluation: add pending vector, then assign positive zero.',0x51d4e6:'Shared model-definition attachment transform initialization.',0x51d544:'Shared model-definition attachment transform initialization.',0x51d568:'Shared model-definition attachment transform initialization.'}
rows=[]
for section in p.sections:
 if section.Characteristics&0x20000000:
  for i in d.disasm(section.get_data(),p.OPTIONAL_HEADER.ImageBase+section.VirtualAddress):
   if i.id and any(o.type==X86_OP_MEM and o.mem.disp in (0x12c0,0x12c4,0x12c8) for o in i.operands):
    assert i.address in classification,hex(i.address)
    rows.append(dict(address=hex(i.address),instruction=i.mnemonic+' '+i.op_str,bytes=i.bytes.hex(),classification=classification[i.address]))
assert {int(r['address'],16) for r in rows}==set(classification)
report=dict(result='PASS',original_sha256=sha,references=rows,scope='Linear executable-section decode with data skipping; direct displacement operands12c0/12c4/12c8 only. Classes were inspected in Ghidra containing functions51ae00/51ae90/51b2e0/51b500/51d420. This is not exhaustive pointer-alias/write analysis and does not prove the vector always stays zero. No nonzero instance producer identified by this scan; no new movement producer is synthesized.')
(root/'artifacts/pose-displacement-references.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(rows),'classified direct references; nonzero producer remains unproven')
